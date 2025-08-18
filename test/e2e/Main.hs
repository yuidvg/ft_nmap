module Main where

import Data.Functor
import Data.IntMap qualified as IM
import Data.Map.Strict qualified as M
import System.Directory ()
import Test.Tasty
import Test.Tasty.HUnit
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

type NmapXMLParseResult = [(HostIP, Ports)]

type Hosts = M.Map HostIP Ports

type ScanResult = Hosts

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
        >>> (normalPortsA &&& extraPortsA)
        >>> arr (uncurry IM.union)

    normalPortsA :: (ArrowXml a) => a XmlTree Ports
    normalPortsA =
      listA
        ( deep (isElem >>> hasName "port")
            >>> ( getAttrValue "portid"
                    &&& getAttrValue "protocol"
                    &&& (getChildren >>> isElem >>> hasName "state" >>> getAttrValue "state")
                    &&& (getChildren >>> isElem >>> hasName "service" >>> getAttrValue "name")
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

tests :: TestTree
tests =
  testGroup
    "ft_nmap e2e parser"
    [ testCase "parse single OPEN line" $
        parseNmapResultFromFile "./test/e2e/nmap_result.xml" >>= \snap1 ->
          parseNmapResultFromFile "./test/e2e/nmap_result.xml" >>= \snap2 ->
            assertEqual "command 1" snap1 snap2
    ]

main :: IO ()
main = defaultMain tests
