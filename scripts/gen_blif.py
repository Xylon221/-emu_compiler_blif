#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_blif.py — 生成合成风格的 BLIF 网表（替代 yosys write_blif）

环境说明：本环境未安装 Yosys（无 sudo 且缺 bison/flex），故用本脚本生成
一个符合 Yosys `write_blif` 输出格式的门级网表，用于复现 emu_compiler 中
BLIF 解析器（blifParse）的性能优化实验。

为贴近真实综合网表的两个关键特征（这也是字符串驻留优化能够奏效的前提）：
  1. 信号名较短 —— 使用 $abc$000123 等内部网名；
  2. 高扇出 / 高重复 —— 每个门的扇入偏向选取"最近产生"的信号，
     使同一信号名被大量门反复引用。
"""

import argparse
import math
import os
import random


MODS = ["dma", "ctrl", "alu", "rf", "fifo", "bus", "mux", "decoder", "arbiter", "pipe"]
KINDS = ["datapath", "state", "rdat", "wdat", "addr", "valid", "ready", "req", "ack"]

# 高扇出"热信号"：真实综合网表中 clk/rst/en/select 等控制信号会被成千上万门引用
HOT = (["clk", "rst", "en", "mode", "ready", "valid", "ack", "req",
        "done", "start", "busy", "empty", "full", "irq"]
       + ["sel_%02d" % i for i in range(16)])


def net_name(seq):
    """Yosys 风格内部网名：$abc$<seq>，短且大量重复（驻留优势最大）。"""
    return "$abc$%06d" % seq


def cube_for(op):
    return {
        "and":  ["11 1"],
        "or":   ["1- 1", "-1 1"],
        "xor":  ["10 1", "01 1"],
        "nand": ["00 1", "01 1", "10 1"],
        "nor":  ["00 1"],
    }[op]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gates", type=int, default=20000, help="组合门数量")
    ap.add_argument("--latches", type=int, default=512, help="触发器数量")
    ap.add_argument("--inputs", type=int, default=64, help="输入信号数量")
    ap.add_argument("--outputs", type=int, default=64, help="输出信号数量")
    ap.add_argument("--fanin-bias", type=float, default=400.0,
                    help="扇入偏向窗口（越小越集中、重复率越高）")
    ap.add_argument("--seed", type=int, default=20260903)
    ap.add_argument("--out", default="blif/dtp_synth.blif")
    args = ap.parse_args()

    if (args.gates < 0 or args.latches < 0 or args.inputs < 1 or
        args.outputs < 0 or args.fanin_bias <= 0):
        ap.error("counts must be nonnegative, inputs>=1, fanin-bias>0")
    rng = random.Random(args.seed)

    inputs = ["dtp_in_%03d" % i for i in range(args.inputs)] + HOT
    pool = list(inputs)

    lines = []
    w = lines.append
    w(".model dtp_top")
    w(".inputs " + " ".join(inputs))

    ops = ["and", "or", "xor", "nand", "nor", "and", "or", "and"]

    # 组合门：扇入以高概率取自"热信号"或"最近产生"的信号，形成高扇出
    for g in range(args.gates):
        def pick():
            # 40% 概率取自热信号池（控制信号高扇出），其余偏向最近信号
            if rng.random() < 0.4:
                return rng.choice(HOT)
            back = int(rng.expovariate(1.0 / args.fanin_bias))
            back = max(0, min(back, len(pool) - 1))
            return pool[len(pool) - 1 - back]
        a, b = pick(), pick()
        out = net_name(g)
        op = ops[g % len(ops)]
        w(".names %s %s %s" % (a, b, out))
        for row in cube_for(op):
            w(row)
        pool.append(out)

    # 时序元素：D 触发器（q 输出重回信号池，被后续逻辑反复引用）
    latches = []
    for i in range(args.latches):
        back = int(rng.expovariate(1.0 / args.fanin_bias))
        back = max(0, min(back, len(pool) - 1))
        d = pool[len(pool) - 1 - back]
        q = "q_%s_%06d" % (KINDS[i % len(KINDS)], i)
        latches.append((d, q))
        pool.append(q)

    for d, q in latches:
        w(".latch %s %s re clk 2" % (d, q))

    # 输出
    chosen = []
    for i in range(args.outputs):
        back = int(rng.expovariate(1.0 / args.fanin_bias))
        back = max(0, min(back, len(pool) - 1))
        chosen.append(pool[len(pool) - 1 - back])
    w(".outputs " + " ".join(chosen))
    w(".end")

    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

    size = os.path.getsize(args.out)
    n_tokens = sum(len(l.split()) for l in lines)
    print("wrote %s  (%.1f KiB, %d tokens, %d gates, %d latches)"
          % (args.out, size / 1024.0, n_tokens, args.gates, args.latches))


if __name__ == "__main__":
    main()
