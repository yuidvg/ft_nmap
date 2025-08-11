{-# LANGUAGE ScopedTypeVariables #-}
module Main where

import Test.Tasty
import Test.Tasty.HUnit
import System.Directory (doesFileExist)
import System.Process (readCreateProcessWithExitCode, proc)
import System.Exit (ExitCode(..))

main :: IO ()
main = defaultMain tests

tests :: TestTree
tests = testGroup "ft_nmap integration (smoke)"
  [ testCase "ft_nmap binary exists" $ do
      let bin = "/workspace/build/bin/ft_nmap"
      exists <- doesFileExist bin
      assertBool ("missing: " ++ bin) exists

  , testCase "nmap --version succeeds" $ do
      (ec, _out, _err) <- readCreateProcessWithExitCode (proc "nmap" ["--version"]) ""
      assertEqual "nmap --version" ExitSuccess ec
  ]


