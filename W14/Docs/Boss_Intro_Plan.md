Boss pre-placed in level
  tag: Boss
  encounter auto-start: false

BossIntroDirector BeginPlay
  find boss
  hide boss
  disable boss collision
  boss:StopEnemyMovement()
  boss HUD hidden

BloodMoonPhase completes
  calls BossIntroDirector.StartIntro()

BossIntroDirector StartIntro
  show boss
  keep collision disabled during cinematic
  manually animate/walk boss
  play camera

BossIntroDirector intro end
  enable collision
  boss:StartBossEncounter()
  show boss HUD
  Game.ExitCutscene()