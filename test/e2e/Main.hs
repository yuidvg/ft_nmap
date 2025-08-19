module Main where

import Control.Monad
import Data.Align
import Data.Functor
import System.FilePath
import Data.IntMap.Strict qualified as IM
import Data.Map.Strict qualified as M
import Data.These
import System.IO.Temp
import System.Process
import Test.Tasty
import Test.Tasty.HUnit
import Text.Show.Pretty
import Text.XML.HXT.Core

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
  = HostOnlyInA Ports
  | HostOnlyInB Ports
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

-- Simple splitOn for single-character delimiters
splitOn :: Char -> String -> [String]
splitOn _ "" = [""]
splitOn delim s =
  let (chunk, rest) = break (== delim) s
   in case rest of
        [] -> [chunk]
        (_ : r) -> chunk : splitOn delim r

portRangeStr2Ports :: String -> [Int]
portRangeStr2Ports s =
  case splitOn '-' s of
    [start, end] -> [read start .. read end]
    _ -> []

portRangesStr2Ports :: String -> [Int]
portRangesStr2Ports s =
  concatMap portRangeStr2Ports (splitOn ',' s)

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
        >>> (normalPortsA &&& (extraPortsA <+> arr (const IM.empty)))
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
      deep (isElem >>> hasName "extraports")
        >>> ( getAttrValue "state"
                &&& (getChildren >>> isElem >>> hasName "extrareasons" >>> (getAttrValue "ports" &&& getAttrValue "proto"))
            )
        >>> arr
          ( \(stateStr, (portsStr, protoStr)) ->
              let state = parsePortState stateStr
                  ports = portRangesStr2Ports portsStr
               in IM.fromList $ map (,PortInfo (parseProtocol protoStr) state "") ports
          )

-- IO boundary: parse from XML string and convert to nested Map
parseNmapResultFromFile :: FilePath -> IO ScanResult
parseNmapResultFromFile xmlPath =
  readFile xmlPath >>= \xmlRaw ->
    runX (readString opts xmlRaw >>> nmapXMLStructure) Data.Functor.<&> mergeHostMaps
  where
    opts = [withValidate no, withSubstDTDEntities no, withRemoveWS yes]
    mergeHostMaps = M.unionsWith IM.union

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

-- ScanResultを再帰的にalignして、新しいデータ構造 AlignedScanResult を返す
deepAlign :: ScanResult -> ScanResult -> AlignedScanResult
deepAlign resA resB =
  let alignedHosts = align resA resB
   in M.map processHost alignedHosts
  where
    processHost :: These Ports Ports -> AlignedHost
    processHost (This portsA) = HostOnlyInA portsA
    processHost (That portsB) = HostOnlyInB portsB
    processHost (These portsA portsB) =
      let alignedPorts = align portsA portsB
       in HostInBoth $ IM.map processPort alignedPorts
      where
        processPort :: These PortInfo PortInfo -> AlignedPort
        processPort (This portA) = PortOnlyInA portA
        processPort (That portB) = PortOnlyInB portB
        processPort (These portA portB) = PortInBoth portA portB

extraHostsOnA :: Hosts -> Hosts -> Hosts
extraHostsOnA = M.difference

extraPortsOnA :: Ports -> Ports -> Ports
extraPortsOnA = IM.difference

pruneHappyPaths :: AlignedScanResult -> AlignedScanResult
pruneHappyPaths = M.mapMaybe pruneHost
  where
    pruneHost :: AlignedHost -> Maybe AlignedHost
    pruneHost (HostInBoth alignedPorts) =
      let -- ポートレベルで差分がないものをフィルタリング
          portDiffs = IM.filter (not . isBothSame) alignedPorts
       in if IM.null portDiffs
            then Nothing -- ポート差分がなければホストごと消す
            else Just (HostInBoth portDiffs)
    -- Aのみ、Bのみのホストは差分なのでそのまま残す
    pruneHost diff = Just diff

    -- PortInfoのstateが同じBothはハッピーパス
    isBothSame :: AlignedPort -> Bool
    isBothSame (PortInBoth a b) = a.state == b.state
    isBothSame _ = False -- This, Thatは差分

-- ---------------------------------------
-- TESTS
-- ---------------------------------------

data TestCase = TestCase
  { nmapArgs :: [String],
    ftNmapArgs :: [String]
  }
  deriving (Show)

testcases :: [TestCase]
testcases =
  [ TestCase
      { ftNmapArgs = ["-sS", "1.1.1.1", "-p", "22,80"],
        nmapArgs = ["-sS", "1.1.1.1", "-p", "22,80,30"]
      },
    TestCase
      { ftNmapArgs = ["-sU", "google.com", "-p", "53"],
        nmapArgs = ["-sU", "google.com", "-p", "53"]
      }
      -- ... add Test Cases Here
  ]

conductTest :: TestCase -> Assertion
conductTest testCase' =
 withSystemTempDirectory "nmap-e2e-test-" $ \tempDir -> do
    -- 作成したディレクトリの中に、出力先のファイルパスを構築する
    let nmapResultPath = tempDir </> "nmap_result.xml"
        ftNmapResultPath = tempDir </> "ft_nmap_result.xml"

    -- nmapとft_nmapをそれぞれ実行し、専用のファイルに書き込ませる
    -- 注意：元のコードでは両方とも "nmap" になっていたので修正
    callProcess "nmap"    (testCase'.nmapArgs    ++ ["-oX", nmapResultPath])
    callProcess "./build/bin/ft_nmap" (testCase'.ftNmapArgs ++ ["-oX", ftNmapResultPath])

    -- 以降の処理は変更なし
    nmapResult <- parseNmapResultFromFile nmapResultPath
    ftNmapResult <- parseNmapResultFromFile ftNmapResultPath

    let scanResultDiff = pruneHappyPaths $ deepAlign nmapResult ftNmapResult

    unless (M.null scanResultDiff) $
      assertFailure ("scanResultDiff: " ++ ppShow scanResultDiff)

tests :: TestTree
tests =
  testGroup
    "RESULT COMPARISON"
    (map (\tc -> testCase (show tc) (conductTest tc)) testcases)

main :: IO ()
main = defaultMain tests
