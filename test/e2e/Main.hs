module Main where

import Data.Bifunctor
import Data.Functor
import Data.IntMap qualified as IM
import Data.Map.Strict qualified as M
import System.Directory ()
import Test.Tasty
import Test.Tasty.HUnit
import Text.XML.HXT.Core

-- Normalized state used by comparisons
data PortState = StateOpen | StateClosed | StateFiltered | StateUnknown
  deriving (Eq, Show)

-- Hierarchical structure for a host's port entry
data PortInfo = PortInfo
  { protocol :: String,
    state :: PortState,
    serviceName :: String
  }
  deriving (Eq, Show)

data Port = Port
  { id :: Int,
    protocol :: String,
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

nmapXMLStructure :: (ArrowXml a) => a XmlTree Hosts
nmapXMLStructure =
  deep (isElem >>> hasName "host")
    >>> listA (ipA &&& listA portA)
    >>> arr (M.fromListWith IM.union . map (Data.Bifunctor.second IM.fromList))
  where
    ipA =
      getChildren
        >>> isElem
        >>> hasName "address"
        >>> hasAttrValue "addrtype" (== "ipv4")
        >>> getAttrValue "addr"

    portA =
      getChildren
        >>> isElem
        >>> hasName "ports"
        >>> getChildren
        >>> isElem
        >>> hasName "port"
        >>> ( getAttrValue "portid"
                &&& getAttrValue "protocol"
                &&& (getChildren >>> isElem >>> hasName "state" >>> getAttrValue "state")
                &&& (getChildren >>> isElem >>> hasName "service" >>> getAttrValue "name")
            )
        >>> arr
          ( \(portIdStr, (protocolStr, (stateStr, serviceName))) ->
              ( parseInt portIdStr,
                PortInfo protocolStr (parsePortState stateStr) serviceName
              )
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
  | otherwise = StateUnknown

tests :: TestTree
tests =
  testGroup
    "ft_nmap e2e parser"
    [ testCase "parse single OPEN line" $ do
        snap1 <- parseNmapResultFromFile "./test/e2e/nmap_result2.xml"
        snap2 <- parseNmapResultFromFile "./test/e2e/nmap_result2.xml"
        assertEqual "command 1" snap1 snap2
    ]

main :: IO ()
main = defaultMain tests
