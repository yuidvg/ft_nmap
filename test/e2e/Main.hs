{-# LANGUAGE ScopedTypeVariables #-}

module Main where

import Data.Char
import Data.List
import Data.Map.Strict qualified as M
import System.Directory
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

-- Parse a single ft_nmap result line like:
--   "OPEN       | 45.33.32.156 80"
parseFtLine :: String -> Maybe (Key, PortState)
parseFtLine rawLine =
  let line = trim rawLine
      (lhs, rest) = break (== '|') line
   in case rest of
        [] -> Nothing
        (_ : ' ' : rhs) ->
          let stateStr = trim lhs
              ws = words rhs
           in case ws of
                (ip : portStr : _) ->
                  case reads portStr of
                    [(p :: Int, _)] ->
                      let st = case stateStr of
                            "OPEN" -> StateOpen
                            "CLOSED" -> StateClosed
                            "FILTERED" -> StateFiltered
                            _ -> StateUnknown
                       in Just ((ip, p), st)
                    _ -> Nothing
                _ -> Nothing
        _ -> Nothing

-- Build snapshot from entire ft_nmap output
parseFtOutput :: String -> Snapshot
parseFtOutput content =
  let pairs = mapMaybe parseFtLine (lines content)
   in M.fromList pairs

-- Parse Nmap XML (-oX) while validating against nmap.dtd using HXT
-- Refactored: pure extraction arrow + thin IO boundary

-- Pure extraction pipeline: XmlTree -> ((ip,port), state)
nmapPairs :: ArrowXml a => a XmlTree ((String, Int), PortState)
nmapPairs =
  deep (isElem >>> hasName "host")
    >>> ( getChildren
            >>> isElem
            >>> hasName "address"
            >>> hasAttrValue "addrtype" (== "ipv4")
            >>> getAttrValue "addr"
            &&& deep
              ( isElem
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
    >>> arr
      ( \(ip, (portStr, stateStr)) ->
          ((ip, parseInt portStr), parsePortState stateStr)
      )

-- IO boundary: parse from XML string
parseNmapResultFromString :: String -> IO Snapshot
parseNmapResultFromString xmlRaw = do
  let dtdUri = "https://svn.nmap.org/nmap/docs/nmap.dtd"
      xmlWithDoctype = injectDoctype dtdUri xmlRaw
      opts =
        [ withValidate yes,
          withSubstDTDEntities yes,
          withRemoveWS yes
        ]
  pairs <- runX (readString opts xmlWithDoctype >>> nmapPairs)
  pure (M.fromList pairs)

-- IO boundary: parse from file path
parseNmapResultFromFile :: FilePath -> IO Snapshot
parseNmapResultFromFile xmlPath = do
  xmlRaw <- readFile xmlPath
  parseNmapResultFromString xmlRaw

-- Backward-compatible name
parseNmapResult :: FilePath -> IO Snapshot
parseNmapResult = parseNmapResultFromFile

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

injectDoctype :: String -> String -> String
injectDoctype dtdUri content =
  let ls = lines content
      doctypeLine = "<!DOCTYPE nmaprun SYSTEM \"" ++ dtdUri ++ "\">"
      isDoctype l = "<!DOCTYPE nmaprun" `isPrefixOf` dropWhile isSpace l
   in if any isDoctype ls
        then
          unlines
            ( map
                (\l -> if isDoctype l then doctypeLine else l)
                ls
            )
        else case ls of
          (l1 : rest)
            | "<?xml" `isPrefixOf` dropWhile isSpace l1 ->
                unlines (l1 : doctypeLine : rest)
          _ -> unlines (doctypeLine : ls)

-- Assert helper
assertHas :: Snapshot -> (Key, PortState) -> Assertion
assertHas snap (k, v) =
  case M.lookup k snap of
    Just v' -> assertEqual ("state for " ++ show k) v v'
    Nothing -> assertFailure ("missing key " ++ show k)

main :: IO ()
main = defaultMain tests

tests :: TestTree
tests =
  testGroup
    "ft_nmap e2e parser"
    [ testCase "parse single OPEN line" $ do
        let snap = parseFtOutput "OPEN       | 45.33.32.156 80\n"
        assertHas snap (("45.33.32.156", 80), StateOpen),
      testCase "parse single CLOSED line" $ do
        let snap = parseFtOutput "CLOSED     | 45.33.32.156 81\n"
        assertHas snap (("45.33.32.156", 81), StateClosed),
      testCase "parse sample file if exists" $ do
        let candidates =
              [ "script/res/correct_output.txt",
                "script/correct_output.txt"
              ]
        existing <- filterM doesFileExist candidates
        case existing of
          (path : _) -> do
            content <- readFile path
            let snap = parseFtOutput content
            assertHas snap (("45.33.32.156", 80), StateOpen)
            assertHas snap (("45.33.32.156", 81), StateClosed)
            assertHas snap (("8.8.8.8", 444), StateFiltered)
            assertHas snap (("8.8.8.8", 443), StateOpen)
          [] -> assertBool "sample file not found; skipping" True,
      testCase "parse Nmap XML (-oX) with DTD if sample exists" $ do
        let xmlPath = "test/e2e/nmap_result sudo nmap 1.1.1.1 google.com -sS -oX"
        ok <- doesFileExist xmlPath
        if ok
          then do
            snap <- parseNmapResult xmlPath
            assertHas snap (("1.1.1.1", 53), StateOpen)
            assertHas snap (("1.1.1.1", 80), StateOpen)
            assertHas snap (("1.1.1.1", 443), StateOpen)
            assertHas snap (("1.1.1.1", 8080), StateOpen)
            assertHas snap (("1.1.1.1", 8443), StateOpen)
          else assertBool "nmap XML not found; skipping" True
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
