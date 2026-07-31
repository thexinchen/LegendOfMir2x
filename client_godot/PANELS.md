# 游戏内面板

所有面板都位于 `scenes/game/panels/`，可在 Godot 编辑器中直接打开 `.tscn`
预览。场景引用的 PNG 均来自原客户端 `proguse.zsdb`，不是运行时占位图。

当前面板：

- `inventory.tscn`：背包
- `player_state.tscn`：人物与装备
- `skill.tscn`：技能
- `minimap.tscn`：小地图
- `npc_chat.tscn`：NPC 对话
- `purchase.tscn`：商店
- `friend_chat.tscn`：好友聊天
- `guild.tscn`：行会
- `team.tscn`：队伍
- `quest.tscn`：任务
- `horse.tscn`：坐骑
- `auction.tscn`：拍卖
- `secured_items.tscn`：安全物品
- `runtime_config.tscn`：运行设置
- `input_string.tscn`：通用输入确认框

`scenes/game/main.tscn` 是游戏内 HUD。角色选择场景收到服务器
`SM_ONLINEOK` 后会进入该场景。核心快捷键为 `B` 背包、`C` 人物、`S`
技能；其他面板快捷键显示在 HUD 聊天区域。

`scenes/game/control_panel.tscn` 是原客户端底部游戏控制面板，包含血量与
魔法柱、聊天记录和输入框、快捷栏开关、收起/展开、属性数值，以及背包、
人物、技能、行会、队伍、任务、坐骑、设置、好友和小地图入口。

远端验证：

```sh
/home/ubuntu/.local/bin/godot4 --path client_godot --headless \
  res://tests/panel_scene_smoke.tscn
```

测试会逐个加载控制面板和全部 15 个对话框，实例化场景，并确认场景中至少
存在一张有效纹理。
