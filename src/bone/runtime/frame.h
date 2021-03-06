﻿#ifndef BONE_RUNTIME_FRAME_H
#define BONE_RUNTIME_FRAME_H
#include "../bone.h"
#include "../util/stack.h"
#include "storage.h"
struct bnObject;
struct bnLambda;
struct bnInterpreter;

/**
 * bnFrameは実行時に変化する状態を保存する構造体です。
 */
typedef struct bnFrame {
        int pc;
        int depth;
        bnStack* vStack;
        bnStack* hierarcySelf;
        GHashTable* variableTable;
        //ラムダ式の内部で宣言された変数の一覧を記録するリスト
        //外側の変数は含まない
        GPtrArray* variableListExcludeOuter;
        bnReference currentCall;
        bnReference panic;
        bnReference register0;
        struct bnFrame* prev;
        struct bnFrame* next;
        GList* snapshots;
} bnFrame;

/**
 * 新しいbnFrameインスタンスを生成して返します。
 * @param type
 * @return
 */
bnFrame* bnNewFrame();

/**
 * 指定のフレームの子フレームを生成して返します。
 * 子フレームのローカル変数もまたルートとしてGCにマークされます。
 * @param self
 * @return
 */
bnFrame* bnSubFrame(bnFrame* self);

/**
 * フレームのスタックの最後の値を返します。
 * @param self
 * @return
 */
bnReference bnReturnValue(bnFrame* self);

/**
 * 入力フレームのローカル変数を全て出力フレームへ書き出します。
 * @param src
 * @param dst
 */
void bnInjectFrame(GHashTable* src, bnFrame* dst);

/**
 * 新しい配列インスタンスを生成し、フレームの全ての変数を書き出します。
 * また、その参照を返します。
 * @param bone
 * @param self
 * @return
 */
bnReference bnExportAllVariable(struct bnInterpreter* bone, bnFrame* self);

/**
 * 指定の名前で変数を書き出します。
 * @param frame
 * @param name
 * @param ref
 */
void bnWriteVariable(bnFrame* frame, bnStringView name, bnReference ref);

/**
 * 指定の名前で変数を書き出します。
 * @param frame
 * @param pool
 * @param name
 * @param obj
 */
void bnWriteVariable2(bnFrame* frame, struct bnStringPool* pool,
                      const char* name, bnReference ref);

/**
 * 指定の名前の変数を読み出します。
 * @param frame
 * @param name
 * @return
 */
bnReference bnReadVariable(bnFrame* frame, bnStringView name);

/**
 * 指定の名前の変数を読み出します。
 * @param frame
 * @param pool
 * @param name
 * @return
 */
bnReference bnReadVariable2(bnFrame* frame, struct bnStringPool* pool,
                            const char* name);

/**
 * 指定の変数を宣言したことにします。
 * これは可変長戻り値のために必要です。
 * def()(...) のような形式の戻り値がインジェクションされる時、
 * 全ての戻り値をインジェクションする必要がありますが、
 * ただ単にテーブルの中身全てをインジェクションしてしまうとラムダの性質のためにキャプチャされた外側の変数もインジェクションされてしまいます。
 * これを回避するために、変数が宣言された場合にはこれを呼び出します。
 * @param frame
 * @param name
 */
void bnDeclareVariable(bnFrame* frame, bnStringView name);

/**
 * ローカル変数とスタックのための領域を解放します。
 * オブジェクトは解放されません。
 * @param self
 */
void bnDeleteFrame(bnFrame* self);
#endif