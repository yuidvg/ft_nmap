# Thread-Safe Memory Management System

## 概要

このメモリ管理システムは、スレッドセーフでfree忘れの心配がないmallocラッパーです。関数型プログラミングの原則に従って設計されており、以下の特徴があります：

- **スレッドセーフ**: 複数のスレッドから同時にアクセスしても安全
- **自動メモリ管理**: 全てのメモリを追跡し、一括でfreeできる
- **エラーハンドリング**: 失敗時の動作が明確で予測可能
- **関数型スタイル**: 純粋関数とimmutableな設計

## API

### 型定義

```c
// メモリ割り当て結果を表す構造体
typedef struct
{
    const bool succeeded;  // 成功したかどうか
    void *const ptr;      // 割り当てられたメモリへのポインタ
} malloc_result_t;
```

### 関数

#### `memory_manager_init()`
```c
malloc_result_t memory_manager_init(void);
```
メモリマネージャを初期化します。
- **戻り値**: 初期化の成功/失敗を示す`malloc_result_t`
- **注意**: 最初の`safe_malloc()`呼び出し時に自動的に初期化されるため、明示的な呼び出しは任意

#### `safe_malloc()`
```c
malloc_result_t safe_malloc(const size_t size);
```
スレッドセーフなメモリ割り当てを行います。
- **引数**: `size` - 割り当てるメモリのサイズ（バイト）
- **戻り値**: 割り当て結果を含む`malloc_result_t`
- **特徴**:
  - 割り当てたメモリは自動的に追跡される
  - サイズが0の場合は失敗する
  - スレッドセーフ

#### `memory_manager_cleanup()`
```c
void memory_manager_cleanup(void);
```
全ての割り当てられたメモリを一括でfreeします。
- **副作用**: 全てのメモリブロックを解放し、メモリマネージャを未初期化状態にする
- **注意**: プログラム終了時やメモリをリセットしたい時に呼び出す

#### `is_memory_manager_initialized()`
```c
bool is_memory_manager_initialized(void);
```
メモリマネージャが初期化されているかを確認します。
- **戻り値**: 初期化されている場合は`true`、そうでなければ`false`
- **純粋関数**: 副作用なし

## 使用例

### 基本的な使い方

```c
#include "internal.h"

int main(void)
{
    // メモリを割り当て
    const malloc_result_t result = safe_malloc(1024);
    if (result.succeeded)
    {
        // メモリを使用
        int *const data = (int *)result.ptr;
        *data = 42;
        printf("Value: %d\n", *data);
    }

    // プログラム終了時に全てのメモリを一括解放
    memory_manager_cleanup();
    return 0;
}
```

### エラーハンドリング

```c
const malloc_result_t result = safe_malloc(size);
if (!result.succeeded)
{
    fprintf(stderr, "Memory allocation failed\n");
    return -1;
}

// 成功した場合のみメモリを使用
use_memory(result.ptr);
```

### マルチスレッド環境

```c
void *worker_thread(void *arg)
{
    // 各スレッドから安全にメモリを割り当て可能
    const malloc_result_t result = safe_malloc(256);
    if (result.succeeded)
    {
        // スレッド固有の作業を実行
        process_data(result.ptr);
    }
    return NULL;
}

int main(void)
{
    pthread_t threads[4];

    // 複数のスレッドを作成
    for (int i = 0; i < 4; ++i)
    {
        pthread_create(&threads[i], NULL, worker_thread, NULL);
    }

    // スレッドの完了を待機
    for (int i = 0; i < 4; ++i)
    {
        pthread_join(threads[i], NULL);
    }

    // 全てのメモリを一括解放
    memory_manager_cleanup();
    return 0;
}
```

## 設計原則

### 関数型プログラミング
- **純粋関数**: `is_memory_manager_initialized()`は副作用なし
- **Immutability**: 構造体のフィールドは`const`で保護
- **明示的な副作用**: メモリ割り当てなどの副作用は`result_t`型でラップ

### スレッドセーフティ
- **ミューテックス**: 内部でpthreadミューテックスを使用
- **原子的操作**: メモリリストの更新は原子的に実行
- **デッドロック回避**: 単一のミューテックスで一貫した順序

### エラーハンドリング
- **明示的な失敗**: 全ての関数は成功/失敗を明確に示す
- **予測可能な動作**: 失敗時の動作が文書化されている
- **リソースリーク防止**: 失敗時も適切にリソースを解放

## 実装詳細

### メモリブロック構造
```c
typedef struct memory_block
{
    void *const ptr;                    // 割り当てられたメモリ
    const size_t size;                  // メモリサイズ
    struct memory_block *const next;    // 次のブロック
} memory_block_t;
```

### メモリマネージャ構造
```c
typedef struct
{
    memory_block_t *head;      // メモリブロックリストの先頭
    pthread_mutex_t mutex;     // スレッドセーフティ用ミューテックス
    const bool initialized;    // 初期化状態
} memory_manager_t;
```

## テスト

テストスイートは以下の項目をカバーしています：

1. **初期化テスト**: メモリマネージャの正常な初期化
2. **基本割り当てテスト**: 様々なサイズのメモリ割り当て
3. **エラーハンドリングテスト**: 不正な入力への対応
4. **スレッドセーフティテスト**: 並行アクセスの安全性
5. **クリーンアップテスト**: 全メモリの正常な解放

テストの実行：
```bash
clang -std=c23 -Wall -Wextra -I./include -pthread -o test_memory test/test_memory.c test/munit.c src/memory.c
./test_memory
```

## 注意事項

1. **メモリの二重解放**: `memory_manager_cleanup()`後は、以前に割り当てたポインタは無効
2. **プログラム終了時**: 必ず`memory_manager_cleanup()`を呼び出してリソースを解放
3. **大きなメモリ**: システムの制限により、非常に大きなメモリ割り当ては失敗する可能性
4. **スレッド終了**: スレッドが終了してもメモリは自動解放されない（明示的なクリーンアップが必要）

## パフォーマンス考慮事項

- **ミューテックス**: 各割り当てでロックが発生するため、頻繁な小さな割り当ては避ける
- **メモリオーバーヘッド**: 各割り当てに対してメタデータ（約24バイト）が必要
- **リストトラバーサル**: クリーンアップ時に全ブロックを順次処理

このメモリ管理システムは、安全性と使いやすさを重視して設計されており、メモリリークやスレッド競合の心配なくC言語でのプログラミングを行うことができます。