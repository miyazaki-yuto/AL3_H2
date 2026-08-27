# モデル制作仕様

この文書は、現在のゲーム実装に合わせて差し替える3Dモデルの名前とサイズをまとめたものです。

## 共通ルール

- 形式は OBJ（`.obj` / `.mtl` / テクスチャ）です。
- 配置先と名前は `Resources/<モデル名>/<モデル名>.obj` に統一します。
- 地面は XZ 平面、上方向は +Y、前方向は +Z です。
- 原点は、キャラクター・弾・爆弾は接地面の中心、平面エフェクトは平面の中心にしてください。
- 平面エフェクトは、両面表示を前提とせず、上面が +Y を向くように作成してください。
- 下記の「基準サイズ」は、OBJを `scale = {1, 1, 1}` で置いたときの推奨サイズです。実際のゲーム内サイズはコードの `scale_` で拡縮します。

## 最優先：キャラクター

| モデル名 | 配置先 | 基準サイズ (X×Y×Z) | ゲーム内の目安 | 備考 |
|---|---|---:|---:|---|
| `Player` | `Resources/Player/Player.obj` | 2×2×2 | 表示は約3×3×3 | 現在cubeの代用。接地原点で作成。正面は+Z。 |
| `Boss` | `Resources/Boss/Boss.obj` | 20×20×20 | 幅・奥行き20 | 当たり判定半径10と一致させる。接地原点。 |
| `Mob` | `Resources/Mob/Mob.obj` | 2.4×2.4×2.4 | 幅・奥行き約2.4 | 当たり判定半径1.2。接地原点。 |

## Player攻撃

| モデル名 | 配置先 | 基準サイズ (X×Y×Z) | 実装上のサイズ・用途 | 備考 |
|---|---|---:|---|---|
| `PlayerBullet` | `Resources/PlayerBullet/PlayerBullet.obj` | 2×2×2 | 通常時 `scale=1.5`、表示約3×3×3 | 弾本体。チャージで最大2倍の大きさ・射程になる。 |
| `PlayerSlash` | `Resources/PlayerSlash/PlayerSlash.obj` | 1×1×1 | 通常時 X=4.5 / Z=3.0 | 地面に沿う平面推奨。+Zが斬撃の前方向。チャージでX/Zが最大2倍。 |
| `PlayerBeam` | `Resources/PlayerBeam/PlayerBeam.obj` | 1×1×1 | 通常時幅2・高さ2・長さ最大1000 | +Z方向に伸ばせる細長いモデル。現在cube代用。 |
| `PlayerBomb` | `Resources/PlayerBomb/PlayerBomb.obj` | 2×2×2 | 通常時 `scale=1.5`、爆発時は最大50 | 時限爆弾の本体。接地させるため原点は底面中心。 |

## Boss攻撃・ギミック

| モデル名 | 配置先 | 基準サイズ (X×Y×Z) | 実装上のサイズ・用途 | 備考 |
|---|---|---:|---|---|
| `BossBullet` | `Resources/BossBullet/BossBullet.obj` | 2×2×2 | `scale=1.0`、半径1 | Gap Shock Wave用。現在cube代用。 |
| `Laser` | `Resources/Laser/Laser.obj` | 幅15×高さ1×長さ1 | Boss回転レーザー | +Zを長さ方向にする。コード側で幅16程度・長さ5000まで拡縮。 |
| `GroundSpears` | `Resources/GroundSpears/GroundSpears.obj` | 幅約11.4×高さ40×奥行き約11.4 | 槍の壁・地面槍 | 当たり判定半径5.7。原点は地面中心、+Yへ伸びる。 |
| `Wall` | `Resources/Wall/Wall.obj` | 任意（推奨：20×10×2） | ステージ障害物 | 将来の弾・突進反射用。原点は底面中心。 |

## 予兆・範囲表示（平面OBJ）

| モデル名 | 配置先 | 基準サイズ (X×Y×Z) | 実装上のサイズ・用途 | 備考 |
|---|---|---:|---|---|
| `PredictionCircle` | `Resources/PredictionCircle/PredictionCircle.obj` | 2×0×2 | スケール値が半径に相当 | 円形予兆。Boss爆発半径100、爆弾半径50、槍・Mobの予兆に使用。 |
| `BoxPredictionCircle` | `Resources/BoxPredictionCircle/BoxPredictionCircle.obj` | 2×0×2 | 突進予兆：幅20、長さ25〜120 | +Z方向へ引き伸ばす直線予兆。Boss幅20に合わせる。 |

## ステージ

| モデル名 | 配置先 | 基準サイズ (X×Y×Z) | 備考 |
|---|---|---:|---|
| `Ground` | `Resources/Ground/Ground.obj` | 500×0.2×500以上 | 現在のBossレーザー射程が長いため、少なくとも500四方を推奨。原点は中央。 |

## 追加すると演出が良くなるモデル

現状のコードでは未接続ですが、後から追加しやすいモデルです。

| モデル名 | 基準サイズ | 用途 |
|---|---:|---|
| `ChargeEffect` | 2×0×2 | Player足元のチャージリング。最大チャージで2〜4倍に拡大。 |
| `ReflectEffect` | 2×2×2 | 弾・ビームの反射地点に出す火花やシールド。 |
| `HitEffect` | 2×2×2 | Boss・Mobへの命中エフェクト。 |
| `ExplosionEffect` | 2×0×2 | 爆弾爆発時の円形エフェクト。 |

## 差し替え方法

既存のモデル名を使う場合は、同名フォルダ内のOBJ・MTL・テクスチャを置き換えるだけで差し替えられます。

新しい名前を使う場合は、`GameScene.cpp` の `Model::CreateFromOBJ("モデル名", false)` を同じ名前に変更してください。
