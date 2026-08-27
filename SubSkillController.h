#pragma once

#include "KamataEngine.h"
#include "SkillTypes.h"

#include <array>

// サブスキルによる攻撃の追加効果を管理する。
class SubSkillController final {
public:
	void SetSubSkills(const std::array<SubSkillType, 2>& subSkills) { subSkills_ = subSkills; }
	int GetStackCount(SubSkillType subSkill) const {
		if (subSkill == SubSkillType::None) return 0;
		return static_cast<int>(subSkills_[0] == subSkill) +
		       static_cast<int>(subSkills_[1] == subSkill);
	}
	bool HasSubSkill(SubSkillType subSkill) const {
		return GetStackCount(subSkill) > 0;
	}
	bool IsReflectActive() const { return HasSubSkill(SubSkillType::Hansho); }
	bool IsChargeActive() const { return HasSubSkill(SubSkillType::Charge); }
	bool IsBerserkActive() const { return HasSubSkill(SubSkillType::Berserk); }
	bool IsIncreaseActive() const { return HasSubSkill(SubSkillType::Increase); }
	bool IsStealActive() const { return HasSubSkill(SubSkillType::Steal); }

private:
	std::array<SubSkillType, 2> subSkills_ = {SubSkillType::None, SubSkillType::None};
};
