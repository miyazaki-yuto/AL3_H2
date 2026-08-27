#include "GameManager.h"
#include "PostProcessCRT.h"

#include <algorithm>

void GameManager::Initialize() {
	audio_ = KamataEngine::Audio::GetInstance();
	titleBgmHandle_ = audio_->LoadWave("Audio/Title_BGM.wav");
	selectBgmHandle_ = audio_->LoadWave("Audio/Select_BGM.wav");
	gameBgmHandle_ = audio_->LoadWave("Audio/Game_BGM.wav");
	clearBgmHandle_ = audio_->LoadWave("Audio/GameClear_BGM.wav");
	overBgmHandle_ = audio_->LoadWave("Audio/GameOver_Noise_BGM.wav");
	ChangeScene(Scene::kTitle);
	pendingScene_ = Scene::kUnknown;
	transitionPhase_ = TransitionPhase::None;
	transitionTimer_ = 0;
}

void GameManager::Update() {
	if (transitionPhase_ != TransitionPhase::None) {
		UpdateSceneTransition();
		return;
	}
	switch (currentScene_) {
	case Scene::kTitle:
		gameTitle_.Update();
		if (gameTitle_.IsFinished()) {
			BeginSceneTransition(Scene::kSelect);
		}
		break;

	case Scene::kSelect:
		selectScene_.Update();
		if (selectScene_.IsFinished()) {
			BeginSceneTransition(Scene::kGame);
		} else if (selectScene_.IsExplanationRequested()) {
			BeginSceneTransition(Scene::kExplanation);
		}
		break;

	case Scene::kExplanation:
		explanationScene_.Update();
		if (explanationScene_.IsBackRequested()) {
			BeginSceneTransition(Scene::kSelect);
		}
		break;

	case Scene::kGame:
		gameScene_.Update();
		if (gameScene_.IsSelectRequested()) {
			BeginSceneTransition(Scene::kSelect);
		} else if (gameScene_.IsClearRequested()) {
			BeginSceneTransition(Scene::kClear);
		} else if (gameScene_.IsGameOverRequested()) {
			BeginSceneTransition(Scene::kOver);
		}
		break;

	case Scene::kClear:
		gameClear_.Update();
		if (gameClear_.IsRetryRequested()) {
			BeginSceneTransition(Scene::kGame);
		} else if (gameClear_.IsTitleRequested()) {
			BeginSceneTransition(Scene::kTitle);
		}
		break;

	case Scene::kOver:
		gameOver_.Update();
		if (gameOver_.IsRetryRequested()) {
			BeginSceneTransition(Scene::kGame);
		} else if (gameOver_.IsTitleRequested()) {
			BeginSceneTransition(Scene::kTitle);
		}
		break;

	case Scene::kUnknown:
	default:
		break;
	}
}

void GameManager::BeginSceneTransition(Scene nextScene) {
	if (transitionPhase_ != TransitionPhase::None || nextScene == currentScene_) {
		return;
	}
	pendingScene_ = nextScene;
	transitionPhase_ = TransitionPhase::Closing;
	transitionTimer_ = 0;
	PostProcessCRT::SetTransitionProgress(0.0f);
}

void GameManager::UpdateSceneTransition() {
	if (transitionPhase_ == TransitionPhase::Closing) {
		++transitionTimer_;
		const float progress = std::clamp(
		    static_cast<float>(transitionTimer_) /
		        static_cast<float>(kTransitionCloseFrames),
		    0.0f, 1.0f);
		PostProcessCRT::SetTransitionProgress(progress);
		if (transitionTimer_ >= kTransitionCloseFrames) {
			ChangeScene(pendingScene_);
			transitionPhase_ = TransitionPhase::Opening;
			transitionTimer_ = 0;
			PostProcessCRT::SetTransitionProgress(1.0f);
		}
		return;
	}

	if (transitionPhase_ == TransitionPhase::Opening) {
		++transitionTimer_;
		const float progress = 1.0f - std::clamp(
		    static_cast<float>(transitionTimer_) /
		        static_cast<float>(kTransitionOpenFrames),
		    0.0f, 1.0f);
		PostProcessCRT::SetTransitionProgress(progress);
		if (transitionTimer_ >= kTransitionOpenFrames) {
			transitionPhase_ = TransitionPhase::None;
			pendingScene_ = Scene::kUnknown;
			transitionTimer_ = 0;
			PostProcessCRT::SetTransitionProgress(0.0f);
		}
	}
}

void GameManager::Draw() {
	switch (currentScene_) {
	case Scene::kTitle:
		gameTitle_.Draw();
		break;

	case Scene::kSelect:
		selectScene_.Draw();
		break;

	case Scene::kExplanation:
		explanationScene_.Draw();
		break;

	case Scene::kGame:
		gameScene_.Draw();
		break;

	case Scene::kClear:
		gameClear_.Draw();
		break;

	case Scene::kOver:
		gameOver_.Draw();
		break;

	case Scene::kUnknown:
	default:
		break;
	}
}

void GameManager::ChangeScene(Scene nextScene) {
	const Scene previousScene = currentScene_;
	currentScene_ = nextScene;

	switch (currentScene_) {
	case Scene::kTitle:
		gameTitle_.Initialize();
		break;

	case Scene::kSelect:
		if (previousScene == Scene::kExplanation) {
			selectScene_.ResumeFromExplanation();
		} else {
			selectScene_.Initialize();
		}
		break;

	case Scene::kExplanation:
		explanationScene_.Initialize();
		break;

	case Scene::kGame:
		gameScene_.Initialize(
		    selectScene_.GetSelectedMainSkill(), selectScene_.GetSelectedSubSkills());
		break;

	case Scene::kClear:
		gameClear_.Initialize();
		break;

	case Scene::kOver:
		gameOver_.Initialize();
		break;

	case Scene::kUnknown:
	default:
		break;
	}
	PlaySceneBgm(currentScene_);
}

void GameManager::PlaySceneBgm(Scene scene) {
	if (audio_ == nullptr) return;
	if (isBgmPlaying_) {
		audio_->StopWave(currentBgmVoiceHandle_);
		isBgmPlaying_ = false;
	}
	uint32_t soundHandle = 0;
	float volume = 0.32f;
	switch (scene) {
	case Scene::kTitle: soundHandle = titleBgmHandle_; break;
	case Scene::kSelect: soundHandle = selectBgmHandle_; break;
	case Scene::kExplanation: soundHandle = selectBgmHandle_; break;
	case Scene::kGame: soundHandle = gameBgmHandle_; volume = 0.38f; break;
	case Scene::kClear: soundHandle = clearBgmHandle_; volume = 0.42f; break;
	case Scene::kOver: soundHandle = overBgmHandle_; volume = 0.48f; break;
	default: return;
	}
	currentBgmVoiceHandle_ = audio_->PlayWave(soundHandle, true, volume);
	isBgmPlaying_ = true;
}
