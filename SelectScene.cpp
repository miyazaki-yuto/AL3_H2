#include "SelectScene.h"

#include "PostProcessCRT.h"

#include "KamataEngine.h"
#include "TransformUtility.h"

#include <cmath>
#include <limits>
#include <numbers>
#include <random>

using namespace KamataEngine;

namespace {

std::mt19937& GetRouletteRandomEngine() {
	static std::mt19937 randomEngine{std::random_device{}()};
	return randomEngine;
}

MainSkillType SelectWeightedMainSkill() {
	// Beam 5%、ほか4種は各23.75%。停止先を先に決め、表示もそこへ着地させる。
	std::discrete_distribution<int> distribution({4, 19, 19, 19, 19});
	constexpr std::array<MainSkillType, 5> kSkills = {
	    MainSkillType::Beam, MainSkillType::Deploy, MainSkillType::Bullet,
	    MainSkillType::Charge, MainSkillType::Slash};
	return kSkills[distribution(GetRouletteRandomEngine())];
}

SubSkillType SelectWeightedSubSkill() {
	// 反翔5%、ほか4種は各23.75%。結果の後付け差し替えは行わない。
	std::discrete_distribution<int> distribution({19, 19, 19, 4, 19});
	constexpr std::array<SubSkillType, 5> kSkills = {
	    SubSkillType::Steal, SubSkillType::Charge, SubSkillType::Increase,
	    SubSkillType::Hansho, SubSkillType::Berserk};
	return kSkills[distribution(GetRouletteRandomEngine())];
}

constexpr Vector2 kButtonUiUpOrigin = {310.0f, 780.0f};
constexpr Vector2 kButtonUiUpSize = {170.0f, 165.0f};

bool IsControllerButtonTriggered(WORD button) {
	const Input* input = Input::GetInstance();
	if (input == nullptr) {
		return false;
	}
	XINPUT_STATE current{};
	XINPUT_STATE previous{};
	return input->GetJoystickState(0, current) && input->GetJoystickStatePrevious(0, previous) &&
	       (current.Gamepad.wButtons & button) != 0 && (previous.Gamepad.wButtons & button) == 0;
}

bool GetLeftStickTriggeredDirection(int& directionX, int& directionY) {
	directionX = 0;
	directionY = 0;
	const Input* input = Input::GetInstance();
	if (input == nullptr) {
		return false;
	}

	XINPUT_STATE current{};
	XINPUT_STATE previous{};
	if (!input->GetJoystickState(0, current) || !input->GetJoystickStatePrevious(0, previous)) {
		return false;
	}
	constexpr float kStickMaximum = 32767.0f;
	// 倒し込み・戻し込み共に少し余裕を持たせ、スティックを厳密に中央まで
	// 戻さなくても次の方向入力を出せるようにする。
	constexpr float kTriggerThreshold = 0.55f;
	constexpr float kReleaseThreshold = 0.50f;
	const float currentX = static_cast<float>(current.Gamepad.sThumbLX) / kStickMaximum;
	const float currentY = static_cast<float>(current.Gamepad.sThumbLY) / kStickMaximum;
	const float previousX = static_cast<float>(previous.Gamepad.sThumbLX) / kStickMaximum;
	const float previousY = static_cast<float>(previous.Gamepad.sThumbLY) / kStickMaximum;
	const bool useHorizontal = std::abs(currentX) >= std::abs(currentY);
	const float currentStrength = useHorizontal ? std::abs(currentX) : std::abs(currentY);
	const float previousStrength = useHorizontal ? std::abs(previousX) : std::abs(previousY);
	if (currentStrength < kTriggerThreshold || previousStrength > kReleaseThreshold) {
		return false;
	}
	if (useHorizontal) {
		directionX = currentX < 0.0f ? -1 : 1;
	} else {
		directionY = currentY > 0.0f ? -1 : 1;
	}
	return true;
}

} // namespace

void SelectScene::Initialize() {
	mainSkillReelModel_.reset(Model::CreateFromOBJ("MainSkillReel", false));
	subSkillReelModel_.reset(Model::CreateFromOBJ("SubSkillReel", false));
	mainSkillReelTransform_.Initialize();
	mainSkillReelTransform_.scale_ = {kReelScale, kReelScale, kReelScale};
	mainSkillReelTransform_.translation_ = {-10.0f, 0.0f, 0.0f};
	for (size_t index = 0; index < subSkillReelTransforms_.size(); ++index) {
		subSkillReelTransforms_[index].Initialize();
		subSkillReelTransforms_[index].scale_ = {kReelScale, kReelScale, kReelScale};
		subSkillReelTransforms_[index].translation_ = {10.0f * static_cast<float>(index), 0.0f, 0.0f};
	}
	reelCamera_.Initialize();
	reelCamera_.translation_ = {0.0f, 0.0f, -32.0f};
	reelCamera_.UpdateMatrix();
	reelCamera_.TransferMatrix();
	SimpleButton::Colors buttonColors;
	// 未選択時は薄くし、マウスホバー・コントローラーフォーカス時だけ明るくする。
	buttonColors.normal = {0.65f, 0.65f, 0.65f, 0.78f};
	buttonColors.hover = {1.25f, 1.25f, 1.25f, 1.0f};
	buttonColors.pressed = {0.75f, 0.75f, 0.75f, 1.0f};
	buttonColors.disabled = {0.40f, 0.40f, 0.40f, 0.65f};
	const uint32_t buttonTexture = TextureManager::Load("ButtonUI.png");
	const uint32_t stopButtonTexture = TextureManager::Load("jugglerButton.png");
	const uint32_t rouletteStartTexture =
	    TextureManager::Load("RouletteStartButton.png");
	const uint32_t startGameTexture =
	    TextureManager::Load("UI/SelectGameStart.png");
	const uint32_t explanationTexture =
	    TextureManager::Load("UI/SelectExplanationButton.png");
	SimpleButton::Colors rouletteStartColors = buttonColors;
	rouletteStartColors.normal = {0.90f, 0.90f, 0.90f, 0.92f};
	rouletteStartColors.hover = {1.18f, 1.18f, 1.18f, 1.0f};
	rouletteStartColors.pressed = {0.78f, 0.78f, 0.70f, 1.0f};
	startReelsButton_.Initialize(
	    rouletteStartTexture, {1085.0f, 330.0f}, {130.0f, 130.0f},
	    rouletteStartColors);
	SimpleButton::Colors startGameColors = buttonColors;
	startGameColors.normal = {0.78f, 0.78f, 0.78f, 0.90f};
	startGameColors.hover = {1.12f, 1.12f, 1.12f, 1.0f};
	startGameColors.pressed = {0.70f, 0.70f, 0.78f, 1.0f};
	startGameButton_.Initialize(
	    startGameTexture, {420.0f, 55.0f}, {440.0f, 75.0f},
	    startGameColors);
	explanationButton_.Initialize(
	    explanationTexture, {980.0f, 82.0f}, {257.0f, 75.0f},
	    startGameColors);
	for (size_t index = 0; index < manualReelButtons_.size(); ++index) {
		manualReelButtons_[index].Initialize(
		    buttonTexture, {305.0f + 290.0f * static_cast<float>(index), 155.0f},
		    {90.0f, 87.0f}, buttonColors);
		manualReelButtons_[index].SetTextureRect(kButtonUiUpOrigin, kButtonUiUpSize);
	}
	for (size_t index = 0; index < stopReelButtons_.size(); ++index) {
		stopReelButtons_[index].Initialize(
		    stopButtonTexture, {305.0f + 290.0f * static_cast<float>(index), 500.0f},
		    {90.0f, 90.0f}, buttonColors);
	}
	SetFocusedButton(0);
	startGameButton_.SetEnabled(true);
	// 初期状態でも各ホイールの正面面と、実際に使用されるスキルを一致させる。
	SetSelectedMainSkill(SelectWeightedMainSkill());
	reelAngle_ = reelTargetAngle_;
	mainSkillReelTransform_.rotation_.x = reelAngle_;
	UpdateWorldTransform(mainSkillReelTransform_);
	for (size_t index = 0; index < selectedSubSkills_.size(); ++index) {
		SetSelectedSubSkill(index, SelectWeightedSubSkill());
		subReelAngles_[index] = subReelTargetAngles_[index];
		subSkillReelTransforms_[index].rotation_.x = subReelAngles_[index];
		UpdateWorldTransform(subSkillReelTransforms_[index]);
	}
	isReelSpinning_ = false;
	reelStopped_ = {true, true, true};
	reelCoastTimers_ = {0, 0, 0};
	isFinished_ = false;
	isExplanationRequested_ = false;
}

void SelectScene::Update() {
	UpdateControllerFocus();
	startReelsButton_.Update();
	startGameButton_.Update();
	explanationButton_.Update();
	for (SimpleButton& manualReelButton : manualReelButtons_) {
		manualReelButton.Update();
	}
	for (SimpleButton& stopReelButton : stopReelButtons_) {
		stopReelButton.Update();
	}
	if (startReelsButton_.ConsumeClick()) {
		StartReels();
	}
	if (startGameButton_.ConsumeClick() && !isReelSpinning_) {
		isFinished_ = true;
	}
	if (explanationButton_.ConsumeClick() && !isReelSpinning_) {
		isExplanationRequested_ = true;
	}
	for (size_t index = 0; index < manualReelButtons_.size(); ++index) {
		if (manualReelButtons_[index].ConsumeClick()) {
			RotateReelManually(index);
		}
	}
	for (size_t index = 0; index < stopReelButtons_.size(); ++index) {
		if (stopReelButtons_[index].ConsumeClick()) {
			StopReel(index);
		}
	}

#ifdef USE_IMGUI
	ImGui::Begin("Skill Select");
	ImGui::Text("SELECT MAIN SKILL");
	ImGui::Separator();
	ImGui::Text("Available now");
	if (ImGui::Selectable("Bullet", selectedMainSkill_ == MainSkillType::Bullet)) {
		SetSelectedMainSkill(MainSkillType::Bullet);
	}
	if (ImGui::Selectable("Slash", selectedMainSkill_ == MainSkillType::Slash)) {
		SetSelectedMainSkill(MainSkillType::Slash);
	}
	if (ImGui::Selectable("Charge", selectedMainSkill_ == MainSkillType::Charge)) {
		SetSelectedMainSkill(MainSkillType::Charge);
	}
	if (ImGui::Selectable("Beam", selectedMainSkill_ == MainSkillType::Beam)) {
		SetSelectedMainSkill(MainSkillType::Beam);
	}
	if (ImGui::Selectable("Deploy (Time Bomb)", selectedMainSkill_ == MainSkillType::Deploy)) {
		SetSelectedMainSkill(MainSkillType::Deploy);
	}
	ImGui::Text("Aim: Arrow Keys / Right Stick, Attack: Hold Space / RT.");
	ImGui::Separator();
	ImGui::Text("SUB SKILL (SELECT UP TO 2)");
	if (ImGui::Selectable("Hansho (Reflect + Flying attacks)",
	        IsSubSkillSelected(SubSkillType::Hansho))) {
		ToggleSubSkill(SubSkillType::Hansho);
	}
	if (ImGui::Selectable("Charge (Hold, then release to strengthen attacks)",
	        IsSubSkillSelected(SubSkillType::Charge))) {
		ToggleSubSkill(SubSkillType::Charge);
	}
	if (ImGui::Selectable("Berserk (Auto-rush, faster and stronger attacks, life steal)",
	        IsSubSkillSelected(SubSkillType::Berserk))) {
		ToggleSubSkill(SubSkillType::Berserk);
	}
	if (ImGui::Selectable("Increase (Grow stronger on every hit, but become slower)",
	        IsSubSkillSelected(SubSkillType::Increase))) {
		ToggleSubSkill(SubSkillType::Increase);
	}
	if (ImGui::Selectable("Steal (Hits grow max HP and body size, but reduce movement)",
	        IsSubSkillSelected(SubSkillType::Steal))) {
		ToggleSubSkill(SubSkillType::Steal);
	}
	if (ImGui::Selectable("Clear selected sub skills", false)) {
		SetSelectedSubSkill(0, SubSkillType::None);
		SetSelectedSubSkill(1, SubSkillType::None);
	}
	ImGui::Separator();
	ImGui::Text("Coming soon");
	ImGui::Separator();
	if (ImGui::Button("Start") && !isReelSpinning_) {
		isFinished_ = true;
	}
	ImGui::End();
#endif

	if (isReelSpinning_ && !reelStopped_[0]) {
		reelAngle_ += kReelSpinSpeed;
	} else if (reelCoastTimers_[0] > 0) {
		const float progress = 1.0f -
		    static_cast<float>(reelCoastTimers_[0]) / kReelCoastFrames;
		const float easedProgress = 1.0f - std::pow(1.0f - progress, 3.0f);
		reelAngle_ = reelCoastStartAngle_ +
		    (reelCoastEndAngle_ - reelCoastStartAngle_) * easedProgress;
		--reelCoastTimers_[0];
		if (reelCoastTimers_[0] == 0) {
			reelAngle_ = reelCoastEndAngle_;
		}
	} else {
		const float angleDifference = std::remainder(
		    reelTargetAngle_ - reelAngle_, std::numbers::pi_v<float> * 2.0f);
		reelAngle_ += angleDifference * kReelAngleInterpolation;
	}
	mainSkillReelTransform_.rotation_.x = reelAngle_;
	UpdateWorldTransform(mainSkillReelTransform_);
	for (size_t index = 0; index < subSkillReelTransforms_.size(); ++index) {
		if (isReelSpinning_ && !reelStopped_[index + 1]) {
			subReelAngles_[index] += kReelSpinSpeed;
		} else if (reelCoastTimers_[index + 1] > 0) {
			const float progress = 1.0f -
			    static_cast<float>(reelCoastTimers_[index + 1]) / kReelCoastFrames;
			const float easedProgress = 1.0f - std::pow(1.0f - progress, 3.0f);
			subReelAngles_[index] = subReelCoastStartAngles_[index] +
			    (subReelCoastEndAngles_[index] - subReelCoastStartAngles_[index]) *
			        easedProgress;
			--reelCoastTimers_[index + 1];
			if (reelCoastTimers_[index + 1] == 0) {
				subReelAngles_[index] = subReelCoastEndAngles_[index];
			}
		} else {
			const float subAngleDifference = std::remainder(
			    subReelTargetAngles_[index] - subReelAngles_[index], std::numbers::pi_v<float> * 2.0f);
			subReelAngles_[index] += subAngleDifference * kReelAngleInterpolation;
		}
		subSkillReelTransforms_[index].rotation_.x = subReelAngles_[index];
		UpdateWorldTransform(subSkillReelTransforms_[index]);
	}
	if (isReelSpinning_ && reelStopped_[0] && reelStopped_[1] && reelStopped_[2] &&
	    reelCoastTimers_[0] == 0 && reelCoastTimers_[1] == 0 && reelCoastTimers_[2] == 0) {
		isReelSpinning_ = false;
		startGameButton_.SetEnabled(true);
	}
}

void SelectScene::ResumeFromExplanation() {
	isFinished_ = false;
	isExplanationRequested_ = false;
	SetFocusedButton(8);
}

void SelectScene::Draw() {
	if (mainSkillReelModel_ == nullptr || subSkillReelModel_ == nullptr) {
		return;
	}
	Model::PreDraw(Model::CullingMode::kNone);
	mainSkillReelModel_->Draw(mainSkillReelTransform_, reelCamera_);
	for (const WorldTransform& subReelTransform : subSkillReelTransforms_) {
		subSkillReelModel_->Draw(subReelTransform, reelCamera_);
	}
	Model::PostDraw();
	Sprite::PreDraw();
	PostProcessCRT::RebindActiveSceneTarget();
	startReelsButton_.Draw();
	startGameButton_.Draw();
	explanationButton_.Draw();
	for (const SimpleButton& manualReelButton : manualReelButtons_) {
		manualReelButton.Draw();
	}
	for (const SimpleButton& stopReelButton : stopReelButtons_) {
		stopReelButton.Draw();
	}
	Sprite::PostDraw();
}

void SelectScene::StartReels() {
	isReelSpinning_ = true;
	startGameButton_.SetEnabled(false);
	reelStopped_ = {false, false, false};
	reelCoastTimers_ = {0, 0, 0};
	// 開始ボタンをAで押した後、そのまま次のA入力で左リールを止められるようにする。
	SetFocusedButton(3);
}

void SelectScene::StopReel(size_t reelIndex) {
	if (!isReelSpinning_ || reelIndex >= reelStopped_.size() || reelStopped_[reelIndex]) {
		return;
	}
	reelStopped_[reelIndex] = true;
	reelCoastTimers_[reelIndex] = kReelCoastFrames;
	constexpr float kFullRotation = std::numbers::pi_v<float> * 2.0f;
	constexpr float kCoastFullRotations = 1.0f;
	if (reelIndex == 0) {
		SetSelectedMainSkill(SelectWeightedMainSkill());
		reelCoastStartAngle_ = reelAngle_;
		float remainingAngle = std::fmod(reelTargetAngle_ - reelAngle_, kFullRotation);
		if (remainingAngle < 0.0f) remainingAngle += kFullRotation;
		reelCoastEndAngle_ =
		    reelAngle_ + kFullRotation * kCoastFullRotations + remainingAngle;
	} else {
		const size_t subSkillIndex = reelIndex - 1;
		SetSelectedSubSkill(subSkillIndex, SelectWeightedSubSkill());
		subReelCoastStartAngles_[subSkillIndex] = subReelAngles_[subSkillIndex];
		float remainingAngle = std::fmod(
		    subReelTargetAngles_[subSkillIndex] - subReelAngles_[subSkillIndex],
		    kFullRotation);
		if (remainingAngle < 0.0f) remainingAngle += kFullRotation;
		subReelCoastEndAngles_[subSkillIndex] =
		    subReelAngles_[subSkillIndex] +
		    kFullRotation * kCoastFullRotations + remainingAngle;
	}
	// 残っている一番左のリールへフォーカスを進め、A連打で順番に停止できるようにする。
	for (size_t nextReelIndex = 0; nextReelIndex < reelStopped_.size(); ++nextReelIndex) {
		if (!reelStopped_[nextReelIndex]) {
			SetFocusedButton(static_cast<int>(nextReelIndex) + 3);
			return;
		}
	}
	// 全リール停止後は、余韻回転が終わればそのままゲーム開始へ進める。
	SetFocusedButton(7);
}

void SelectScene::RotateReelManually(size_t reelIndex) {
	if (isReelSpinning_ || reelIndex >= reelStopped_.size()) {
		return;
	}
	const float nextAngleStep = std::numbers::pi_v<float> * 2.0f / 5.0f;
	if (reelIndex == 0) {
		SetSelectedMainSkill(GetMainSkillAtAngle(reelTargetAngle_ + nextAngleStep));
	} else {
		const size_t subSkillIndex = reelIndex - 1;
		SetSelectedSubSkill(
		    subSkillIndex, GetSubSkillAtAngle(subReelTargetAngles_[subSkillIndex] + nextAngleStep));
	}
}

void SelectScene::UpdateControllerFocus() {
	int directionX = 0;
	int directionY = 0;
	if (IsControllerButtonTriggered(XINPUT_GAMEPAD_DPAD_LEFT)) {
		directionX = -1;
	} else if (IsControllerButtonTriggered(XINPUT_GAMEPAD_DPAD_RIGHT)) {
		directionX = 1;
	} else if (IsControllerButtonTriggered(XINPUT_GAMEPAD_DPAD_UP)) {
		directionY = -1;
	} else if (IsControllerButtonTriggered(XINPUT_GAMEPAD_DPAD_DOWN)) {
		directionY = 1;
	} else {
		GetLeftStickTriggeredDirection(directionX, directionY);
	}
	if (directionX == 0 && directionY == 0) {
		return;
	}

	const Vector2 focusedCenter = GetButtonCenter(focusedButtonIndex_);
	float closestDistanceSquared = (std::numeric_limits<float>::max)();
	int nextButtonIndex = focusedButtonIndex_;
	for (int buttonIndex = 0; buttonIndex < 9; ++buttonIndex) {
		if (buttonIndex == focusedButtonIndex_) {
			continue;
		}
		const Vector2 candidateCenter = GetButtonCenter(buttonIndex);
		const float differenceX = candidateCenter.x - focusedCenter.x;
		const float differenceY = candidateCenter.y - focusedCenter.y;
		if ((directionX < 0 && differenceX >= 0.0f) ||
		    (directionX > 0 && differenceX <= 0.0f) ||
		    (directionY < 0 && differenceY >= 0.0f) ||
		    (directionY > 0 && differenceY <= 0.0f)) {
			continue;
		}
		const float distanceSquared = differenceX * differenceX + differenceY * differenceY;
		if (distanceSquared < closestDistanceSquared) {
			closestDistanceSquared = distanceSquared;
			nextButtonIndex = buttonIndex;
		}
	}
	SetFocusedButton(nextButtonIndex);
}

void SelectScene::SetFocusedButton(int buttonIndex) {
	if (buttonIndex < 0 || buttonIndex >= 9) {
		return;
	}
	focusedButtonIndex_ = buttonIndex;
	startReelsButton_.SetFocused(buttonIndex == 6);
	startGameButton_.SetFocused(buttonIndex == 7);
	explanationButton_.SetFocused(buttonIndex == 8);
	for (size_t index = 0; index < manualReelButtons_.size(); ++index) {
		manualReelButtons_[index].SetFocused(buttonIndex == static_cast<int>(index));
		stopReelButtons_[index].SetFocused(buttonIndex == static_cast<int>(index) + 3);
	}
}

Vector2 SelectScene::GetButtonCenter(int buttonIndex) const {
	const SimpleButton* button = nullptr;
	if (buttonIndex < 3) {
		button = &manualReelButtons_[buttonIndex];
	} else if (buttonIndex < 6) {
		button = &stopReelButtons_[buttonIndex - 3];
	} else if (buttonIndex == 6) {
		button = &startReelsButton_;
	} else if (buttonIndex == 7) {
		button = &startGameButton_;
	} else {
		button = &explanationButton_;
	}
	return {
		button->GetPosition().x + button->GetSize().x * 0.5f,
		button->GetPosition().y + button->GetSize().y * 0.5f,
	};
}

void SelectScene::SetSelectedMainSkill(MainSkillType mainSkill) {
	selectedMainSkill_ = mainSkill;
	reelTargetAngle_ = GetReelTargetAngle(mainSkill);
}

void SelectScene::SetSelectedSubSkill(size_t slotIndex, SubSkillType subSkill) {
	if (slotIndex >= selectedSubSkills_.size()) {
		return;
	}
	selectedSubSkills_[slotIndex] = subSkill;
	subReelTargetAngles_[slotIndex] = GetReelTargetAngle(subSkill);
}

float SelectScene::GetReelTargetAngle(MainSkillType mainSkill) const {
	// OBJの正面面はBeam。実表示で確認した面の並びは
	// Beam -> Deploy -> Bullet -> Charge -> Slash。
	int reelIndex = 0;
	switch (mainSkill) {
	case MainSkillType::Beam: reelIndex = 0; break;
	case MainSkillType::Deploy: reelIndex = 1; break;
	case MainSkillType::Bullet: reelIndex = 2; break;
	case MainSkillType::Charge: reelIndex = 3; break;
	case MainSkillType::Slash: reelIndex = 4; break;
	default: break;
	}
	return -static_cast<float>(reelIndex) * (std::numbers::pi_v<float> * 2.0f / 5.0f);
}

float SelectScene::GetReelTargetAngle(SubSkillType subSkill) const {
	// OBJの正面面はSteal。実表示で確認した面の並びは
	// Steal -> Charge -> Increase -> Reflect -> Berserk。
	int reelIndex = 0;
	switch (subSkill) {
	case SubSkillType::Steal: reelIndex = 0; break;
	case SubSkillType::Charge: reelIndex = 1; break;
	case SubSkillType::Increase: reelIndex = 2; break;
	case SubSkillType::Hansho: reelIndex = 3; break;
	case SubSkillType::Berserk: reelIndex = 4; break;
	case SubSkillType::None: reelIndex = 0; break;
	default: break;
	}
	return -static_cast<float>(reelIndex) * (std::numbers::pi_v<float> * 2.0f / 5.0f);
}

MainSkillType SelectScene::GetMainSkillAtAngle(float angle) const {
	constexpr std::array<MainSkillType, 5> kSkills = {
	    MainSkillType::Beam, MainSkillType::Deploy, MainSkillType::Bullet,
	    MainSkillType::Charge, MainSkillType::Slash,
	};
	float smallestDifference = (std::numeric_limits<float>::max)();
	MainSkillType selectedSkill = kSkills[0];
	for (const MainSkillType skill : kSkills) {
		const float difference = std::abs(std::remainder(
		    angle - GetReelTargetAngle(skill), std::numbers::pi_v<float> * 2.0f));
		if (difference < smallestDifference) {
			smallestDifference = difference;
			selectedSkill = skill;
		}
	}
	return selectedSkill;
}

SubSkillType SelectScene::GetSubSkillAtAngle(float angle) const {
	constexpr std::array<SubSkillType, 5> kSkills = {
	    SubSkillType::Steal, SubSkillType::Charge, SubSkillType::Increase,
	    SubSkillType::Hansho, SubSkillType::Berserk,
	};
	float smallestDifference = (std::numeric_limits<float>::max)();
	SubSkillType selectedSkill = kSkills[0];
	for (const SubSkillType skill : kSkills) {
		const float difference = std::abs(std::remainder(
		    angle - GetReelTargetAngle(skill), std::numbers::pi_v<float> * 2.0f));
		if (difference < smallestDifference) {
			smallestDifference = difference;
			selectedSkill = skill;
		}
	}
	return selectedSkill;
}

bool SelectScene::IsSubSkillSelected(SubSkillType subSkill) const {
	return selectedSubSkills_[0] == subSkill || selectedSubSkills_[1] == subSkill;
}

void SelectScene::ToggleSubSkill(SubSkillType subSkill) {
	for (SubSkillType& selectedSubSkill : selectedSubSkills_) {
		if (selectedSubSkill == subSkill) {
			SetSelectedSubSkill(static_cast<size_t>(&selectedSubSkill - selectedSubSkills_.data()),
			                     SubSkillType::None);
			return;
		}
	}
	for (size_t index = 0; index < selectedSubSkills_.size(); ++index) {
		if (selectedSubSkills_[index] == SubSkillType::None) {
			SetSelectedSubSkill(index, subSkill);
			return;
		}
	}
}
