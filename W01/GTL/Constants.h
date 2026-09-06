#pragma once
#include <Windows.h>

const float GRAVITY = 9.8f / 3.0f;

const float PI = 3.14159265359f;
const float DEG2RAD = PI / 180.0f;
const int MAX_BALLS = 256;

const float GROUND_HEIGHT = -0.52f;
const float PLAYER_HEIGHT = 1.0f;
const float PLAYER_LEG_LENGTH = 0.075f;		// ? 레? 어 중심부로 ???발까지??거리
const float PLAYER_MOVESPEED = 0.3f;
const float PLAYER_JUMP_POWER = 1.55f;
const int PLAYER_KICK_STARTUP_FRAME = 4;	// ? 레? 어 ??? 작부분의 ? 니메이??길이 (? 레??? 위)
const int PLAYER_KICK_RECOVERY_FRAME = 20;	// ? 레? 어 ??종료부분의 ? 니메이??길이 (? 레??? 위)
const float PLAYER_KICK_ANGLE = 120.0f;
const int PLAYER_WALK_FRAME = 20;			// ? 레? 어 걷기 ? 니메이??길이(? 레??? 위)
const float PLAYER_WALK_WIDTH = 0.0375f; 		// ? 레? 어 걷기 ? 니메이? 의 보폭
const float DASH_TAP_THRESHOLD = 0.25f; 
const float DASH_COOLDOWN = 0.6f;
const float DASH_POWER = 1.5f;
const float DASH_DURATION = 0.2f;

// 1p 조작
const int KEY_1P_MOVE_LEFT = 'A';
const int KEY_1P_MOVE_RIGHT = 'D';
const int KEY_1P_JUMP = 'W';
const int KEY_1P_KICK = VK_SPACE;
const int KEY_1P_SKILL = VK_LSHIFT;
// 2p 조작
const int KEY_2P_MOVE_LEFT = VK_LEFT;
const int KEY_2P_MOVE_RIGHT = VK_RIGHT;
const int KEY_2P_JUMP = VK_UP;
const int KEY_2P_KICK = VK_RSHIFT;
const int KEY_2P_SKILL = VK_OEM_2; // slash (/)

const float BALL_SOUND_THRESHOLD_VELOCITY = 0.1f;
const float BALL_SOUND_THRESHOLD_DISPLACEMENT = 0.00001f;

enum class EBodyType
{
    Static,
    Kinematic,
    Dynamic
};

enum class ECollisionShape
{
	Circle,
	AABB
};

enum class ECollisionMode //         浹    ?
{
	Block,
	Trigger,
	Ignore
};

enum class EPairInteraction //  浹     ȣ ۿ      
{
	None,
	Trigger,
	Block
};

enum GameState {
	Playing,
	Paused,
	GameOver
};

enum EAction
{
	MoveLeft,
	MoveRight,
	Jump,
	Kick,
	Skill,
};

enum ETextAlign 
{
	Left, 
	Center, 
	Right 
};

struct FActionKeyPair
{
	EAction action;
	int keycode;
};

struct FInputState
{
	bool Down;
	bool Hold;
	bool Up;
};

namespace BallConstants {
	constexpr float BALL_SIZE = 0.08f;

	constexpr float BALL_RESTITUTION = 0.9f;
}

namespace GM_defaults {
	const float InitialTimeLeft = 60.0f;
	const float leftPlayerX		= -0.67f;
	const float leftPlayerY		= -0.422f;
	const float rightPlayerX	= 0.67f;
	const float rightPlayerY	= -0.422f;
	const float kickoffX		= 0.0f;
	const float kickoffY		= 0.15f;

	const float goalHeight		= 0.58f;		// Height of the goal area
	const float goalWidth		= 0.15f;	// Width of the goal area
	const float goalY			= -0.72f;
	const float leftGoalX		= -0.92f;	// X position of the left goal
	const float rightGoalX		= 0.77f;	// X position of the right goal

	const float kickoffPause	= 3.0f;		// Time to pause before kickoff
}

// Particles_____________________________
const float particleLaunchSpeed		= 0.9f;
const float particleMaxRadius		= 0.03f;
const float particleBrightening		= 0.25f;

enum ParticleType {
	Outward,
	Inward
};
// Particles_____________________________

// SKills________________________________
const float maxSkillGauge = 100.0f;
const float skillGaugeIncreaseRate	= 0.125f;		// per frame
const float skillGaugeCompensation	 = 25.0f;		// Additional gauge bonus when opponent scores a goal, to help the losing player make a comeback
const float skillGaugeDecreaseRate	= 20.0f;	// per second when skill is active
const float skillStandByDuration	= 2.0f;		// Time the skill activates after being put on standby

namespace SkillGaugeConstants {
	const float gaugeWidth = 0.7f;	// Width of the skill gauge when rendered
	const float gaugeHeight = 0.05f;	// Height of the skill gauge when rendered
	const float gaugeBackgroundColor[4] = { 0.8f, 0.8f, 0.8f, 1.0f };	// RGBA color for the gauge background
	const float gaugeFillColor[4]		= { 0.9f, 0.1f, 0.1f, 1.0f };		// RGBA color for the filled portion of the gauge
	const float player1GaugePosX = -0.55f;	// X position to render player 1's skill gauge
	const float player1GaugePosY = 0.45f;	// Y position to render player 1's skill gauge
	const float player2GaugePosX = 0.55f;	// X position to render player 2's skill gauge
	const float player2GaugePosY = 0.45f;	// Y position to render player 2's skill gauge

	const int numProgressbar = 50;		// Number of each segment in the skill gauge
}

namespace SkillAConstants {
	const float recoveryDuration	= 0.5f;	// Time after the skill effect ends during which the player cannot move or act
	const float player1NewPositionX	= -0.8f;	// Teleport player to the specified coordinates when the skill is launched
	const float player1NewPositionY	= 0.07f;
	const float player2NewPositionX = 0.85f; // Teleport player to the specified coordinates when the skill is launched
	const float player2NewPositionY = 0.07f;

	const float ball1NewPositionX	= -0.65f;	// Teleport ball to the specified coordinates when the skill is launched
	const float ball1NewPositionY	= -0.05f;
	const float ball2NewPositionX	= 0.65f;	// Teleport ball to the specified coordinates when the skill is launched
	const float ball2NewPositionY	= -0.05f;

	const float ball1LaunchVelocityX = 5.5f;	// Launch the ball in a specific direction with a specific speed when the skill is launched
	const float ball1LaunchVelocityY = 0.05f;

	const float ball2LaunchVelocityX = -5.5f;	// Launch the ball in a specific direction with a specific speed when the skill is launched
	const float ball2LaunchVelocityY = 0.05f;
}

namespace SkillBConstants {
	const float recoveryDuration = 0.5f;	// Time after the skill effect ends during which the player cannot move or act
	const float player1NewPositionX = -0.8f;	// Teleport player to the specified coordinates when the skill is launched
	const float player1NewPositionY = 0.07f;
	const float player2NewPositionX = 0.85f; // Teleport player to the specified coordinates when the skill is launched
	const float player2NewPositionY = 0.07f;

	const float ball1NewPositionX = -0.65f;	// Teleport ball to the specified coordinates when the skill is launched
	const float ball1NewPositionY = -0.05f;
	const float ball2NewPositionX = 0.65f;	// Teleport ball to the specified coordinates when the skill is launched
	const float ball2NewPositionY = -0.05f;

	const float ball1LaunchVelocityX = 5.5f;	// Launch the ball in a specific direction with a specific speed when the skill is launched
	const float ball1LaunchVelocityY = 0.05f;

	const float ball2LaunchVelocityX = -5.5f;	// Launch the ball in a specific direction with a specific speed when the skill is launched
	const float ball2LaunchVelocityY = 0.05f;
}
// SKills________________________________

const int AUDIO_MAX_PLAY_FOR_EACH_CLIP = 8;