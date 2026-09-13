# 日本語ドキュメント（このフォーク独自）

このディレクトリは **[PVi1/esphome-twc-control](https://github.com/PVi1/esphome-twc-control) のフォークに、日本向けの日本語解説を足したもの**。

**原著作：PVi1 — MIT ライセンス。** 本体のコード（`twc-control.yaml`）とその英語コメントは**原文のまま触っていない**。

---

## なぜ本体を和訳しないのか

**上流の更新を取り込めなくなるから。**

`twc-control.yaml` は **2100 行あり、そのうち 6 割がコメント**。しかもそのコメントには「なぜこうしたか」「どのバグでこう直したか」という実測の経緯が詰まっていて、**本体そのものと同じくらい価値がある**。

ここを和訳して上書きすると、**上流が 1 行直すたびに衝突する**。このリポジトリをフォークした目的のひとつが「上流の改善を追い続けること」なので、それを潰す改造はしない。

→ **新規ファイルとして解説を置く。** 衝突しないし、コードと一緒に移動する。

## ブランチ構成

| ブランチ | 用途 |
|---|---|
| `master` | **上流追随専用。触らない。** |
| `jp` | 日本向けの変更と、この日本語ドキュメント |

上流を取り込むとき：

```bash
git fetch upstream
git switch master && git merge upstream/master && git push origin master
git switch jp && git merge master
```

GitHub 上でも「This branch is N commits behind PVi1:master」と出るので、更新の有無が一目でわかる。

## 収録

| ファイル | 内容 |
|---|---|
| [`control-law.md`](control-law.md) | **公表則（`recompute_ct`）の全体解説。** データの流れ、主要変数、中核の 3 行、R1 ハードフロア、`desired_avail` の作り方、エスカレーション、ゾーンステアリング、フェイルセーフ、デバッグ用 CSV ログ、そして**日本向けに変える必要がある 4 点** |

## 日本向けの差分（設計中）

| # | 項目 | 状態 |
|---|---|---|
| 1 | **相関項の再構成**（遅いメーター対応の中核） | 設計中 |
| 2 | 3 相 → 単相 3 線式 | 設計中 |
| 3 | Shelly → echonetlite2mqtt | 未着手 |
| 4 | 定数（ブレーカー 20A → 上限 16A） | 未着手 |

**ハード側は改造不要。** 基板（Waveshare ESP32-S3-RS485-CAN）・ピン配（GPIO17/18/21）・`flash_size: 16MB`・`psram: mode: octal` はいずれも上流のままで一致している。

## 背景資料

設計判断の経緯・実測データ・先行プロジェクト（tesla-loadpilot / esphome-twc-control / tesla-wall-connector-control）の調査は、**別途プライベートに管理している**。本ディレクトリには、このフォークを読むのに必要な内容だけを置く。
