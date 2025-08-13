{-# LANGUAGE ScopedTypeVariables #-}

module Main where

import Data.Char
import Data.List
import Data.Map.Strict qualified as M
import System.Directory ()
import Test.Tasty
import Test.Tasty.HUnit
import Text.XML.HXT.Core

-- HTTP handler import removed; we prefer local data-files

-- Normalized state used by comparisons
data PortState = StateOpen | StateClosed | StateFiltered | StateUnknown
  deriving (Eq, Show)

type Key = (String, Int) -- (ip, port)

type Snapshot = M.Map Key PortState

trim :: String -> String
trim = dropWhileEnd isSpace . dropWhile isSpace

-- Pure extraction pipeline: XmlTree -> ((ip,port), state)
nmapPairs :: (ArrowXml a) => a XmlTree ((String, Int), PortState)
nmapPairs =
  deep (isElem >>> hasName "host")
    >>> ( listA
            ( getChildren
                >>> isElem
                >>> hasName "address"
                >>> hasAttrValue "addrtype" (== "ipv4")
                >>> getAttrValue "addr"
            )
            &&&
            listA
              ( getChildren
                  >>> isElem
                  >>> hasName "ports"
                  >>> getChildren
                  >>> isElem
                  >>> hasName "port"
                  >>> ( getAttrValue "portid"
                          &&& ( getChildren
                                  >>> isElem
                                  >>> hasName "state"
                                  >>> getAttrValue "state"
                              )
                      )
              )
        )
    >>> arrL
      ( \(ips, portPairs) ->
          [ ((ip, parseInt portStr), parsePortState stateStr)
          | ip <- ips
          , (portStr, stateStr) <- portPairs
          ]
      )

-- IO boundary: parse from XML string
parseNmapResultFromFile :: FilePath -> IO Snapshot
parseNmapResultFromFile xmlPath = do
  xmlRaw <- readFile xmlPath
  let opts =
        [ withValidate no
        , withSubstDTDEntities no
        , withRemoveWS yes
        ]
  pairs <- runX (readString opts xmlRaw >>> nmapPairs)
  pure (M.fromList pairs)

parsePortState :: String -> PortState
parsePortState s
  | s == "open" = StateOpen
  | s == "closed" = StateClosed
  | s == "filtered" = StateFiltered
  | otherwise = StateUnknown

parseInt :: String -> Int
parseInt s =
  case reads s of
    [(n, _)] -> n
    _ -> 0

-- Assert helper
assertHas :: Snapshot -> (Key, PortState) -> Assertion
assertHas snap (k, v) =
  case M.lookup k snap of
    Just v' -> assertEqual ("state for " ++ show k) v v'
    Nothing -> assertFailure ("missing key " ++ show k)

main :: IO ()
main = do
  snapDbg <- parseNmapResultFromFile "/Users/yui/Developer/42/ft_nmap/test/e2e/nmap_result.xml"
  print (M.toList snapDbg)
  defaultMain tests

tests :: TestTree
tests =
  testGroup
    "ft_nmap e2e parser"
    [ testCase "parse single OPEN line" $ do
        snap <- parseNmapResultFromFile "/Users/yui/Developer/42/ft_nmap/test/e2e/nmap_result.xml"
        print (M.toList snap)
        assertHas snap (("1.1.1.1", 53), StateOpen)
    ]

-- small utilities (without bringing extra deps)
mapMaybe :: (a -> Maybe b) -> [a] -> [b]
mapMaybe f = foldr (\x acc -> maybe acc (: acc) (f x)) []

filterM :: (Monad m) => (a -> m Bool) -> [a] -> m [a]
filterM p =
  foldr
    ( \x r -> do
        b <- p x
        xs <- r
        pure (if b then x : xs else xs)
    )
    (pure [])
