#pragma once

// プレイヤーが選択するメインスキル。現在は Bullet を実装済み。
enum class MainSkillType {
	Bullet,
	Slash,
	Deploy,
	Charge,
	Beam,
};

enum class SubSkillType {
	None,
	Hansho,
	Charge,
	Berserk,
	Increase,
	Steal,
};
