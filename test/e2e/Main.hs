module Main where

import Control.Monad
import Data.Align
import Data.Char (isDigit)
import Data.Functor
import Data.IntMap.Strict qualified as IM
import Data.List.NonEmpty qualified as NE
import Data.List.Split
import Data.Map.Strict qualified as M
import Data.Maybe
import Data.These
import System.Process
import Test.Tasty
import Test.Tasty.HUnit
import Text.Show.Pretty
import Text.XML.HXT.Core
import Data.List qualified as L

-- aliases
type NonEmptyString = NE.NonEmpty Char

-- Normalized state used by comparisons
data PortState = StateOpen | StateClosed | StateFiltered | StateUnknown | StateOpenFiltered | StateClosedFiltered
  deriving (Eq, Show)

-- Hierarchical structure for a host's port entry
data PortInfo = PortInfo
  { protocol :: Protocol,
    state :: PortState,
    serviceName :: String
  }
  deriving (Eq, Show)

data Protocol = UDP | TCP
  deriving (Eq, Show)

data Port = Port
  { id :: Int,
    protocol :: Protocol,
    state :: PortState,
    serviceName :: String
  }
  deriving (Eq, Show)

-- New nested Map structure
type HostIP = String

type Ports = IM.IntMap PortInfo

type Hosts = M.Map HostIP Ports

type ScanResult = Hosts

-- for diff
data AlignedHost
  = HostOnlyInNmap Ports
  | HostOnlyInFtNmap Ports
  | HostInBoth (IM.IntMap AlignedPort) -- ポートレベルでalignした結果
  deriving (Show)

data AlignedPort
  = PortOnlyInA PortInfo
  | PortOnlyInB PortInfo
  | PortInBoth PortInfo PortInfo
  deriving (Show)

type AlignedScanResult = M.Map HostIP AlignedHost

-- ---------------------------------------
-- PARSER
-- ---------------------------------------

parseInt :: String -> Int
parseInt s =
  case reads s of
    [(n, _)] -> n
    _ -> 0

parseProtocol :: String -> Protocol
parseProtocol s
  | s == "udp" = UDP
  | otherwise = TCP

portRangeStr2Ports :: String -> [Int]
portRangeStr2Ports s =
  case splitOn "-" s of
    [start, end] -> [read start .. read end]
    [port] -> [read port]
    _ -> []

portRangesStr2Ports :: String -> [Int]
portRangesStr2Ports s =
  concatMap portRangeStr2Ports (splitOn "," s)

nmapXMLStructure :: (ArrowXml a) => a XmlTree ScanResult
nmapXMLStructure =
  deep (isElem >>> hasName "host")
    >>> listA (ipA &&& portsA)
    >>> arr M.fromList
  where
    ipA =
      getChildren
        >>> isElem
        >>> hasName "address"
        >>> hasAttrValue "addrtype" (== "ipv4")
        >>> getAttrValue "addr"

    portsA =
      getChildren
        >>> isElem
        >>> hasName "ports"
        >>> (normalPortsA &&& extraPortsA)
        >>> arr (uncurry IM.union)

    normalPortsA :: (ArrowXml a) => a XmlTree Ports
    normalPortsA =
      listA
        ( deep (isElem >>> hasName "port")
            >>> ( getAttrValue "portid"
                    &&& getAttrValue "protocol"
                    &&& (getChildren >>> isElem >>> hasName "state" >>> getAttrValue "state")
                    &&& ( (getChildren >>> isElem >>> hasName "service" >>> getAttrValue "name")
                            <+> arr (const "")
                        )
                )
            >>> arr
              ( \(portIdStr, (protocolStr, (stateStr, serviceName))) ->
                  ( parseInt portIdStr,
                    PortInfo (parseProtocol protocolStr) (parsePortState stateStr) serviceName
                  )
              )
        )
        >>> arr IM.fromList

    extraPortsA :: (ArrowXml a) => a XmlTree Ports
    extraPortsA =
      listA
        ( deep (isElem >>> hasName "extraports")
            >>> ( getAttrValue "state"
                    &&& (getChildren >>> isElem >>> hasName "extrareasons" >>> (getAttrValue "ports" &&& getAttrValue "proto"))
                )
            >>> arr
              ( \(stateStr, (portsStr, protoStr)) ->
                  let state = parsePortState stateStr
                      ports = portRangesStr2Ports portsStr
                   in IM.fromList $ Prelude.map (,PortInfo (parseProtocol protoStr) state "") ports
              )
        )
        >>> arr (foldr IM.union IM.empty)

-- IO boundary: parse from XML string and convert to nested Map
parseNmapResult :: String -> IO ScanResult
parseNmapResult xmlRaw = runX (readString opts xmlRaw >>> nmapXMLStructure) Data.Functor.<&> mergeHostMaps
  where
    opts = [withValidate no, withSubstDTDEntities no, withRemoveWS yes]
    mergeHostMaps = M.unionsWith IM.union

-- 新しいトップレベルのパーサー関数
parseFtNmapResult :: String -> ScanResult
parseFtNmapResult input =
  let sections = splitByIPSection input
      parsedSections = Prelude.map parseIPSection sections
   in M.fromList parsedSections

splitByIPSection :: String -> [NonEmptyString]
splitByIPSection whole =
  let sections :: [String] = drop 1 $ splitOn "█ IP:" whole
   in mapMaybe NE.nonEmpty sections

nonEmptyLines :: NonEmptyString -> NE.NonEmpty String
nonEmptyLines = fromJust . NE.nonEmpty . lines . NE.toList

parseIPSection :: NonEmptyString -> (HostIP, Ports)
parseIPSection section =
  let (ipLine NE.:| restLines) = nonEmptyLines section
      portLines =
        Prelude.filter
          ( \case
              (c : _) -> isDigit c
              [] -> False
          )
          restLines
      ports = IM.fromList $ Prelude.map parsePortLine portLines
      ip = parseIPLine ipLine
   in (ip, ports)

parsePortLine :: String -> (Int, PortInfo)
parsePortLine line =
  case splitOn " " line of
    [portNum, protoStr, stateStr, serviceName] ->
      (parseInt portNum, PortInfo (parseProtocol protoStr) (parsePortState stateStr) serviceName)
    _ -> (-1, PortInfo TCP StateUnknown "")

parseIPLine :: String -> HostIP
parseIPLine ipLine =
  let parts = splitOn " " ipLine
      _ = print ("ipLine: " ++ show ipLine)
      _ = print ("parts: " ++ show parts)
      ipPart = case drop 1 parts of
        (ip : _) -> ip
        [] -> ""
   in ipPart

parseFtNmapResultFromFile :: FilePath -> IO ScanResult
parseFtNmapResultFromFile outputPath = do
  outputRaw <- readFile outputPath
  return $ parseFtNmapResult outputRaw

parsePortState :: String -> PortState
parsePortState s
  | s == "open" = StateOpen
  | s == "closed" = StateClosed
  | s == "filtered" = StateFiltered
  | s == "open|filtered" = StateOpenFiltered
  | s == "closed|filtered" = StateClosedFiltered
  | otherwise = StateUnknown

-- ---------------------------------------
-- DIFF
-- ---------------------------------------

deepAlign :: ScanResult -> ScanResult -> AlignedScanResult
deepAlign resA resB =
  let alignedHosts = align resA resB
   in M.map processHost alignedHosts
  where
    processHost :: These Ports Ports -> AlignedHost
    processHost (This portsA) = HostOnlyInNmap portsA
    processHost (That portsB) = HostOnlyInFtNmap portsB
    processHost (These portsA portsB) =
      let alignedPorts = align portsA portsB
       in HostInBoth $ IM.map processPort alignedPorts
      where
        processPort :: These PortInfo PortInfo -> AlignedPort
        processPort (This portA) = PortOnlyInA portA
        processPort (That portB) = PortOnlyInB portB
        processPort (These portA portB) = PortInBoth portA portB

pruneHappyPaths :: AlignedScanResult -> AlignedScanResult
pruneHappyPaths = M.mapMaybe pruneHost
  where
    pruneHost :: AlignedHost -> Maybe AlignedHost
    pruneHost (HostInBoth alignedPorts) =
      let portDiffs = IM.filter (not . isBothSame) alignedPorts
       in if IM.null portDiffs
            then Nothing
            else Just (HostInBoth portDiffs)
    pruneHost diff = Just diff

    isBothSame :: AlignedPort -> Bool
    isBothSame (PortInBoth a b) = a.state == b.state
    isBothSame _ = False

-- ---------------------------------------
-- TESTS
-- ---------------------------------------

data TestCase = TestCase
  { testName :: String,
    nmapArgs :: [String],
    ftNmapArgs :: [String]
  }
  deriving (Show)

testcases :: [TestCase]
testcases = [
   TestCase
      { testName = "SYN with large port range",
        nmapArgs = ["-sS", "-Pn", "1.1.1.1", "-p", "1-1024"],
        ftNmapArgs = ["--scan", "SYN", "--ip", "1.1.1.1", "--port", "1-1024"]
      },
    -- TestCase
    --   { testName = "UDP with large port range",
    --     nmapArgs = ["-sU", "-Pn", "1.1.1.1", "-p", "1-1024"],
    --     ftNmapArgs = ["--scan", "UDP", "--ip", "1.1.1.1", "--port", "1-1024"]
    --   },
    TestCase
      { testName = "ACK with large port range",
        nmapArgs = ["-sA", "-Pn", "1.1.1.1", "-p", "1-1024"],
        ftNmapArgs = ["--scan", "ACK", "--ip", "1.1.1.1", "--port", "1-1024"]
      },
    TestCase
      { testName = "XMAS with large port range",
        nmapArgs = ["-sX", "-Pn", "1.1.1.1", "-p", "1-1024"],
        ftNmapArgs = ["--scan", "XMAS", "--ip", "1.1.1.1", "--port", "1-1024"]
      },
    TestCase
      { testName = "FIN with large port range",
        nmapArgs = ["-sF", "-Pn", "1.1.1.1", "-p", "1-1024"],
        ftNmapArgs = ["--scan", "FIN", "--ip", "1.1.1.1", "--port", "1-1024"]
      },
    TestCase
      { testName = "NULL with large port range",
        nmapArgs = ["-sN", "-Pn", "1.1.1.1", "-p", "1-1024"],
        ftNmapArgs = ["--scan", "NULL", "--ip", "1.1.1.1", "--port", "1-1024"]
      },
    TestCase
      { testName = "SYN with domain name",
        nmapArgs = ["-sS", "-Pn", "google.com", "-p", "1-1024"],
        ftNmapArgs = ["--scan", "SYN", "--ip", "google.com", "--port", "1-1024"]
      }
      ,
    -- TestCase
    --   { testName = "UDP with domain name",
    --     nmapArgs = ["-sU", "-Pn", "google.com", "-p", "1-1024"],
    --     ftNmapArgs = ["--scan", "UDP", "--ip", "google.com", "--port", "1-1024"]
    --   }
    --   ,
    TestCase
      { testName = "ACK with domain name",
        nmapArgs = ["-sA", "-Pn", "google.com", "-p", "1-1024"],
        ftNmapArgs = ["--scan", "ACK", "--ip", "google.com", "--port", "1-1024"]
      }
      ,
    TestCase
      { testName = "XMAS with domain name",
        nmapArgs = ["-sX", "-Pn", "google.com", "-p", "1-1024"],
        ftNmapArgs = ["--scan", "XMAS", "--ip", "google.com", "--port", "1-1024"]
      }
      ,
    TestCase
      { testName = "FIN with domain name",
        nmapArgs = ["-sF", "-Pn", "google.com", "-p", "1-1024"],
        ftNmapArgs = ["--scan", "FIN", "--ip", "google.com", "--port", "1-1024"]
      }
      ,
    TestCase
      { testName = "NULL with domain name",
        nmapArgs = ["-sN", "-Pn", "google.com", "-p", "1-1024"],
        ftNmapArgs = ["--scan", "NULL", "--ip", "google.com", "--port", "1-1024"]
      }
      ,
    TestCase
      { testName = "SYN with high port range",
        nmapArgs = ["-sS", "-Pn", "1.1.1.1", "-p", "65530-65535"],
        ftNmapArgs = ["--scan", "SYN", "--ip", "1.1.1.1", "--port", "65530-65535"]
      }
      ,
    -- TestCase
    --   { testName = "UDP with high port range",
    --     nmapArgs = ["-sU", "-Pn", "1.1.1.1", "-p", "65530-65535"],
    --     ftNmapArgs = ["--scan", "UDP", "--ip", "1.1.1.1", "--port", "65530-65535"]
    --   }
    --   ,
    TestCase
      { testName = "ACK with high port range",
        nmapArgs = ["-sA", "-Pn", "1.1.1.1", "-p", "65530-65535"],
        ftNmapArgs = ["--scan", "ACK", "--ip", "1.1.1.1", "--port", "65530-65535"]
      }
      ,
    TestCase
      { testName = "XMAS with high port range",
        nmapArgs = ["-sX", "-Pn", "1.1.1.1", "-p", "65530-65535"],
        ftNmapArgs = ["--scan", "XMAS", "--ip", "1.1.1.1", "--port", "65530-65535"]
      }
      ,
    TestCase
      { testName = "FIN with high port range",
        nmapArgs = ["-sF", "-Pn", "1.1.1.1", "-p", "65530-65535"],
        ftNmapArgs = ["--scan", "FIN", "--ip", "1.1.1.1", "--port", "65530-65535"]
      }
      ,
    TestCase
      { testName = "NULL with high port range",
        nmapArgs = ["-sN", "-Pn", "1.1.1.1", "-p", "65530-65535"],
        ftNmapArgs = ["--scan", "NULL", "--ip", "1.1.1.1", "--port", "65530-65535"]
      }
      ,
    TestCase
      { testName = "SYN with scanme",
        nmapArgs = ["-sS", "-Pn", "scanme.nmap.org", "-p", "65530-65535"],
        ftNmapArgs = ["--scan", "SYN", "--ip", "scanme.nmap.org", "--port", "65530-65535"]
      }
      ,
    -- TestCase
    --   { testName = "UDP with scanme",
    --     nmapArgs = ["-sU", "-Pn", "scanme.nmap.org", "-p", "65530,65534,65535"],
    --     ftNmapArgs = ["--scan", "UDP", "--ip", "scanme.nmap.org", "--port", "65530,65534,65535"]
    --   }
    --   ,
    TestCase
      { testName = "ACK with scanme",
        nmapArgs = ["-sA", "-Pn", "scanme.nmap.org", "-p", "65530-65535"],
        ftNmapArgs = ["--scan", "ACK", "--ip", "scanme.nmap.org", "--port", "65530-65535"]
      }
      ,
    TestCase
      { testName = "XMAS with scanme",
        nmapArgs = ["-sX", "-Pn", "scanme.nmap.org", "-p", "65530-65535"],
        ftNmapArgs = ["--scan", "XMAS", "--ip", "scanme.nmap.org", "--port", "65530-65535"]
      }
      ,
    TestCase
      { testName = "FIN with scanme",
        nmapArgs = ["-sF", "-Pn", "scanme.nmap.org", "-p", "65530-65535"],
        ftNmapArgs = ["--scan", "FIN", "--ip", "scanme.nmap.org", "--port", "65530-65535"]
      }
      ,
    TestCase
      { testName = "NULL with scanme",
        nmapArgs = ["-sN", "-Pn", "scanme.nmap.org", "-p", "65530-65535"],
        ftNmapArgs = ["--scan", "NULL", "--ip", "scanme.nmap.org", "--port", "65530-65535"]
      }
      -- ... add Test Cases Here
  ]

conductTest :: TestCase -> Assertion
conductTest testCase' =
  do
    nmapOutputRaw <- readProcess "nmap" (testCase'.nmapArgs ++ ["-oX", "/dev/stdout", "--noninteractive"]) ""
    let nmapOutput = trimToXML nmapOutputRaw
    ftNmapOutput <- readProcess "./build/bin/ft_nmap" (testCase'.ftNmapArgs ++ ["--speedup", "250" , "--test"]) ""
    -- putStrLn $ "nmapOutput: " ++ nmapOutput  -- debug snippet
    nmapResult <- parseNmapResult nmapOutput
    let ftNmapResult = parseFtNmapResult ftNmapOutput

    -- debug
    -- putStrLn $ "nmapResult: " ++ ppShow nmapResult
    -- putStrLn $ "ftNmapResult: " ++ ppShow ftNmapResult

    let scanResultDiff = pruneHappyPaths $ deepAlign nmapResult ftNmapResult

    unless (M.null scanResultDiff) $
      assertFailure ("scanResultDiff: " ++ ppShow scanResultDiff)

tests :: TestTree
tests =
  testGroup
    "RESULT COMPARISON"
    (Prelude.map (\tc -> testCase tc.testName (conductTest tc)) testcases)

main :: IO ()
main = defaultMain tests

trimToXML :: String -> String
trimToXML =
  unlines               -- 行リスト → 文字列
  . takeWhile (not . L.isPrefixOf "Nmap done")
  . lines               -- 文字列 → 行リスト
  . dropWhile (/= '<')  -- prolog ('<') までスキップ
