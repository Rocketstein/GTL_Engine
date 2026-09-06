#pragma once
#include "Core/CoreTypes.h"
#include "Windows.h"
#include <vector>
//enum : Key or Moust Btn
enum class EInputEventType : unsigned char
{
	KeyDown,
	KeyUp,
	MouseButtonDown,
	MouseButtonUp,
	MouseWheel,
};
// Struct : Input Event
// Fuction : Store Input Event information of Type and Key/Button
struct FInputEvent
{
	EInputEventType Type;
	int32 KeyOrButton;
	float Value = 0.0f;
};

// InputManager : Handle raw input from windows 
// and provide interface to query key and mouse state
class FInputManager
{
public:
	static FInputManager& Get()
	{
		if (Instance == nullptr)
			Instance = new FInputManager();
		return *Instance;
	}
	static void Shutdown()
	{
		if (Instance)
		{
			delete Instance;
			Instance = nullptr;
		}
	}

	FInputManager(const FInputManager&) = delete;
	FInputManager(FInputManager&&) = delete;
	FInputManager& operator=(const FInputManager&) = delete;
	FInputManager& operator=(FInputManager&&) = delete;

	void ProcessMessage(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);
	void Tick();

	bool IsKeyDown(int32 Key) const;
	bool IsKeyPressed(int32 Key) const;
	bool IsKeyReleased(int32 Key) const;

	// Mouse specific 
	bool IsMouseButtonDown(int32 Button) const;
	bool IsMouseButtonPressed(int32 Button) const;
	bool IsMouseButtonReleased(int32 Button) const;

	// Mouse position & Delta
	POINT GetMousePos() const { return LastMousePos; }
	float GetMouseDeltaX() const { return MouseDeltaX; }
	float GetMouseDeltaY() const { return MouseDeltaY; }
	bool MouseMoved() const { return std::abs(MouseDeltaX) > 1e-6f || std::abs(MouseDeltaY) > 1e-6f; }

	// Scrolling
	float GetMouseWheelDelta() const { return MouseWheelDelta; }
	bool ScrolledUp() const { return MouseWheelDelta > 0.0f; }
	bool ScrolledDown() const { return MouseWheelDelta < 0.0f; }
	float GetScrollNotches() const { return MouseWheelDelta; } // Already normalized

	// Dragging
	bool IsDragging(int32 Button) const;
	bool WasDragStarted(int32 Button) const;
	bool WasDragEnded(int32 Button) const;
	POINT GetDragDelta(int32 Button) const;
	float GetDragDistance(int32 Button) const;

	void SetTrackingMouse(bool bTrack) { bTrackingMouse = bTrack; if (bTrack) GetCursorPos(&LastMousePos); }
	void SetLastMousePos(POINT Pos) { LastMousePos = Pos; }
	void SetOwnerWindow(HWND Hwnd) { OwnerHWnd = Hwnd; }
	bool IsWindowFocused() const { return bWindowFocused; }

	// GUI state helpers (wrappers for ImGui check)
	bool IsGuiUsingMouse() const;
	bool IsGuiUsingKeyboard() const;
	void SetGuiCaptureOverride(bool bInUsingMouse, bool bInUsingKeyboard, bool bInUsingTextInput);
	void ClearGuiCaptureOverride();

	// Resets
	void ResetMouseDelta();
	void ResetWheelDelta();
	void ResetAllKeyStates();
	void ResetAllStates();

	// Win32 indices

	static constexpr int32 MOUSE_LEFT = VK_LBUTTON;
	static constexpr int32 MOUSE_RIGHT = VK_RBUTTON;
	static constexpr int32 MOUSE_MIDDLE = VK_MBUTTON;
	static constexpr int32 MOUSE_X1 = VK_XBUTTON1;
	static constexpr int32 MOUSE_X2 = VK_XBUTTON2;

private:
	FInputManager() = default;
	~FInputManager() = default;

	static constexpr int32 MAX_KEYS = 256;
	static constexpr int32 DRAG_THRESHOLD = 5;

	// Event queue (filled by WndProc, flushed in Tick)
	std::vector<FInputEvent> EventQueue;

	bool KeyState[MAX_KEYS] = {};
	bool PrevKeyState[MAX_KEYS] = {};

	float MouseDeltaX = 0.0f;
	float MouseDeltaY = 0.0f;
	float MouseWheelDelta = 0.0f;
	float PendingWheelDelta = 0.0f;

	float RawMouseDeltaAccumX = 0.0f;
	float RawMouseDeltaAccumY = 0.0f;

	POINT LastMousePos = {};
	bool bTrackingMouse = false;

	// Dragging state 
	bool bIsDragging[MAX_KEYS] = {};
	bool bWasDragStarted[MAX_KEYS] = {};
	bool bWasDragEnded[MAX_KEYS] = {};
	POINT MouseDownPos[MAX_KEYS] = {};
	bool bDragCandidate[MAX_KEYS] = {};

	HWND OwnerHWnd = nullptr;
	bool bWindowFocused = true;
	bool bHasGuiCaptureOverride = false;
	bool bGuiUsingMouseOverride = false;
	bool bGuiUsingKeyboardOverride = false;
	bool bGuiUsingTextInputOverride = false;

	void UpdateDragging();
	

	static FInputManager* Instance;
};

