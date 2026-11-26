extends CanvasLayer

onready var startGame = $start
onready var againButton = $again
onready var quitButton = $quit
onready var statusLabel = $status

var buttons = []
var last_navigation_ms = -1000
var startup_until_ms = 0

# Avoid double focus moves from one stick nudge (or duplicate D-pad events).
# Ignore stick motion during startup in case the controller reports an
# initial axis value before it settles.
func _input(event):
	if event is InputEventJoypadMotion and OS.get_ticks_msec() < startup_until_ms:
		get_tree().set_input_as_handled()
		return

	var navigation_event = (event.is_action_pressed("ui_up") or
		event.is_action_pressed("ui_down") or
		event.is_action_pressed("ui_left") or
		event.is_action_pressed("ui_right"))
	if navigation_event:
		var now = OS.get_ticks_msec()
		if now - last_navigation_ms < 120:
			get_tree().set_input_as_handled()
			return
		last_navigation_ms = now

# Called when the node enters the scene tree for the first time.
func _ready():
	startup_until_ms = OS.get_ticks_msec() + 500
	buttons = [startGame, againButton, quitButton]

	startGame.connect("pressed", self, "startGame")
	againButton.connect("pressed", self, "again")
	quitButton.connect("pressed", self, "quit")

	# Left/right cycle through the list; up/down follow the visual order
	# without wrapping at the top or bottom.
	for i in range(buttons.size()):
		var previous = buttons[(i - 1 + buttons.size()) % buttons.size()]
		var following = buttons[(i + 1) % buttons.size()]

		var top_neighbour = NodePath()
		if i > 0:
			top_neighbour = buttons[i].get_path_to(previous)

		var bottom_neighbour = NodePath()
		if i < buttons.size() - 1:
			bottom_neighbour = buttons[i].get_path_to(following)

		buttons[i].set_focus_neighbour(MARGIN_LEFT, buttons[i].get_path_to(previous))
		buttons[i].set_focus_neighbour(MARGIN_RIGHT, buttons[i].get_path_to(following))
		buttons[i].set_focus_neighbour(MARGIN_TOP, top_neighbour)
		buttons[i].set_focus_neighbour(MARGIN_BOTTOM, bottom_neighbour)

	startGame.grab_focus()

func startGame():
	get_tree().change_scene("res://other.tscn")

func again():
	statusLabel.text = "Again selected (genuinely how did you find this?)"

func quit():
	statusLabel.text = "Quitting the game.."
	OS.execute("python3", ["-c", "print('a')"], true)
	get_tree().quit()

# Called every frame. 'delta' is the elapsed time since the previous frame.
#func _process(delta):
#	pass
