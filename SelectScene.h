#pragma once

#include "KamataEngine.h"
#include "SimpleButton.h"
#include "SkillTypes.h"

#include <array>
#include <memory>

class SelectScene final {
public:
	void Initialize();
	void ResumeFromExplanation();
	void Update();
	void Draw();

	bool IsFinished() const { return isFinished_; }
	bool IsExplanationRequested() const { return isExplanationRequested_; }
	MainSkillType GetSelectedMainSkill() const { return selectedMainSkill_; }
	const std::array<SubSkillType, 2>& GetSelectedSubSkills() const { return selectedSubSkills_; }

private:
	void SetSelectedMainSkill(MainSkillType mainSkill);
	void SetSelectedSubSkill(size_t slotIndex, SubSkillType subSkill);
	void StartReels();
	void StopReel(size_t reelIndex);
	void RotateReelManually(size_t reelIndex);
	void UpdateControllerFocus();
	void SetFocusedButton(int buttonIndex);
	KamataEngine::Vector2 GetButtonCenter(int buttonIndex) const;
	MainSkillType GetMainSkillAtAngle(float angle) const;
	SubSkillType GetSubSkillAtAngle(float angle) const;
	float GetReelTargetAngle(MainSkillType mainSkill) const;
	float GetReelTargetAngle(SubSkillType subSkill) const;

	MainSkillType selectedMainSkill_ = MainSkillType::Bullet;
	void ToggleSubSkill(SubSkillType subSkill);
	bool IsSubSkillSelected(SubSkillType subSkill) const;

	std::array<SubSkillType, 2> selectedSubSkills_ = {SubSkillType::None, SubSkillType::None};
	std::unique_ptr<KamataEngine::Model> mainSkillReelModel_;
	std::unique_ptr<KamataEngine::Model> subSkillReelModel_;
	KamataEngine::WorldTransform mainSkillReelTransform_;
	std::array<KamataEngine::WorldTransform, 2> subSkillReelTransforms_;
	KamataEngine::Camera reelCamera_;
	float reelAngle_ = 0.0f;
	float reelTargetAngle_ = 0.0f;
	float reelCoastStartAngle_ = 0.0f;
	float reelCoastEndAngle_ = 0.0f;
	std::array<float, 2> subReelAngles_ = {0.0f, 0.0f};
	std::array<float, 2> subReelTargetAngles_ = {0.0f, 0.0f};
	std::array<float, 2> subReelCoastStartAngles_ = {0.0f, 0.0f};
	std::array<float, 2> subReelCoastEndAngles_ = {0.0f, 0.0f};
	SimpleButton startReelsButton_;
	SimpleButton startGameButton_;
	SimpleButton explanationButton_;
	std::array<SimpleButton, 3> manualReelButtons_;
	std::array<SimpleButton, 3> stopReelButtons_;
	int focusedButtonIndex_ = 0;
	bool isReelSpinning_ = false;
	std::array<bool, 3> reelStopped_ = {true, true, true};
	std::array<int, 3> reelCoastTimers_ = {0, 0, 0};
	bool isFinished_ = false;
	bool isExplanationRequested_ = false;

	inline static constexpr float kReelScale = 4.5f;
	inline static constexpr float kReelAngleInterpolation = 0.16f;
	inline static constexpr float kReelSpinSpeed = 0.26f;
	inline static constexpr int kReelCoastFrames = 30;
};
