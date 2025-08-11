{-# LANGUAGE ScopedTypeVariables #-}

module Main where

import Data.Char (isSpace)
import Data.List (dropWhileEnd)
import Data.Map.Strict qualified as M
import System.Directory (doesFileExist)
import Test.Tasty
import Test.Tasty.HUnit

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
          [] -> assertBool "sample file not found; skipping" True
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
