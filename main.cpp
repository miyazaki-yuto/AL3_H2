#include "GameManager.h"
#include "KamataEngine.h"
#include "PostProcessCRT.h"
#include <Windows.h>

using namespace KamataEngine;

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
	Initialize(L"LE2B_27_ミヤザキ_ユウト_ルビルド");

	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	ImGuiManager* imGuiManager = ImGuiManager::GetInstance();

	GameManager gameManager;
	gameManager.Initialize();
	PostProcessCRT crtFilter;
	crtFilter.Initialize();

	while (true) {
		if (Update()) {
			break;
		}

		imGuiManager->Begin();
		gameManager.Update();
		crtFilter.Update();
		imGuiManager->End();

		// 描画処理
		dxCommon->PreDraw();
		crtFilter.BeginScene();
		gameManager.Draw();
		// Sprite::PostDrawがバックバッファへ出した2D UIを透明レイヤーとして取得する。
		crtFilter.CaptureSpriteLayer();
		// Sprite描画後もImGuiを同じオフスクリーンへ含める。
		crtFilter.RebindSceneTarget();
		imGuiManager->Draw();
		crtFilter.EndSceneAndDraw();
		dxCommon->PostDraw();
	}

	Finalize();
	return 0;
}
