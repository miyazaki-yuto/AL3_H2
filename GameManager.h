#pragma once

#include "GameClear.h"
#include "GameOver.h"
#include "GameScene.h"
#include "GameTitle.h"
#include "ExplanationScene.h"
#include "SelectScene.h"

enum class Scene {
	kUnknown = 0,
	kTitle,
	kSelect,
	kExplanation,
	kGame,
	kClear,
	kOver,
};

class GameManager {
public:
	GameManager() = default;
	~GameManager() = default;

	void Initialize();
	void Update();
	void Draw();

private:
	void ChangeScene(Scene nextScene);
	void PlaySceneBgm(Scene scene);
	void BeginSceneTransition(Scene nextScene);
	void UpdateSceneTransition();

	enum class TransitionPhase {
		None,
		Closing,
		Opening,
	};

	Scene currentScene_ = Scene::kTitle;
	Scene pendingScene_ = Scene::kUnknown;
	TransitionPhase transitionPhase_ = TransitionPhase::None;
	int transitionTimer_ = 0;
	KamataEngine::Audio* audio_ = nullptr;
	uint32_t titleBgmHandle_ = 0;
	uint32_t selectBgmHandle_ = 0;
	uint32_t gameBgmHandle_ = 0;
	uint32_t clearBgmHandle_ = 0;
	uint32_t overBgmHandle_ = 0;
	uint32_t currentBgmVoiceHandle_ = 0;
	bool isBgmPlaying_ = false;
	inline static constexpr int kTransitionCloseFrames = 12;
	inline static constexpr int kTransitionOpenFrames = 12;
	GameTitle gameTitle_;
	SelectScene selectScene_;
	ExplanationScene explanationScene_;
	GameScene gameScene_;
	GameClear gameClear_;
	GameOver gameOver_;
};
