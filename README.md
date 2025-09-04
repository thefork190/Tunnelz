# Tunnelz
A UE5 (version 5.6) mobile reflex game.  The game makes use of mobile devices' IMU sensors data to detect wrist flick gestures which are used for gameplay.
  
The game is an endless runner played in portrait mode.  The goal is to collect incapacitated enemies to get the highest score possible.  Tapping an enemy will incapacitate them.  A wrist flick off the phone's right axis will kill and "collect" all incapacitated enemies which will higher the score (+1 per enemy).  Enemies will try to collide with the player and a wrist flick off the phone's up axis will strafe the player left/right.
  
For testing, the game can be played in the editor with a mouse.  Hold left mouse button and rapidly move the mouse left/right to strage.  Left click enemies to encapacitate.  Right click to collect enemies.  Note that there is no cooldowns for these actions in the editor as apposed to playing on an physical device.