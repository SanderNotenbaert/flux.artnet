{
	"patcher": {
		"fileversion": 1,
		"appversion": {
			"major": 8,
			"minor": 2,
			"revision": 0,
			"architecture": "arm64",
			"modernui": 1
		},
		"rect": [
			63.0,
			104.0,
			640.0,
			390.0
		],
		"bglocked": 0,
		"openinpresentation": 0,
		"default_fontsize": 12.0,
		"default_fontface": 0,
		"default_fontname": "Lato Light",
		"gridonopen": 1,
		"gridsize": [
			5.0,
			5.0
		],
		"gridsnaponopen": 2,
		"objectsnaponopen": 0,
		"statusbarvisible": 2,
		"toolbarvisible": 1,
		"lefttoolbarpinned": 2,
		"toptoolbarpinned": 2,
		"righttoolbarpinned": 2,
		"bottomtoolbarpinned": 2,
		"toolbars_unpinned_last_save": 15,
		"tallnewobj": 0,
		"boxanimatetime": 200,
		"enablehscroll": 1,
		"enablevscroll": 1,
		"devicewidth": 0.0,
		"description": "",
		"digest": "",
		"tags": "",
		"style": "tap",
		"subpatcher_template": "tap.template",
		"boxes": [
			{
				"box": {
					"id": "title",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						20.0,
						20.0,
						620.0,
						30.0
					],
					"text": "Scheduled transmission",
					"fontsize": 18.0
				}
			},
			{
				"box": {
					"id": "desc",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						20.0,
						55.0,
						620.0,
						40.0
					],
					"text": "@framerate 40 sends every configured universe 40 times per second, even when unchanged.\nOrdinary updates only change the stored data; they do not transmit immediately in scheduled mode.",
					"fontsize": 12.0
				}
			},
			{
				"box": {
					"id": "send",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"bang",
						""
					],
					"patching_rect": [
						40.0,
						110.0,
						400.0,
						23.0
					],
					"text": "flux.artnet.send @host 127.0.0.1 @universes 0 @framerate 40"
				}
			},
			{
				"box": {
					"id": "values",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						40.0,
						145.0,
						160.0,
						23.0
					],
					"text": "0 255 0 255 0 255"
				}
			},
			{
				"box": {
					"id": "bang",
					"maxclass": "button",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						"bang"
					],
					"patching_rect": [
						220.0,
						145.0,
						24.0,
						24.0
					]
				}
			},
			{
				"box": {
					"id": "receive",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						"list"
					],
					"patching_rect": [
						40.0,
						210.0,
						400.0,
						23.0
					],
					"text": "flux.artnet.receive @universes 0 @framerate 10"
				}
			},
			{
				"box": {
					"id": "gate",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						40.0,
						255.0,
						80.0,
						23.0
					],
					"text": "gate 1"
				}
			},
			{
				"box": {
					"id": "slice",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						140.0,
						255.0,
						120.0,
						23.0
					],
					"text": "zl slice 6"
				}
			},
			{
				"box": {
					"id": "print",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 0,
					"outlettype": [],
					"patching_rect": [
						140.0,
						290.0,
						110.0,
						23.0
					],
					"text": "print first-6"
				}
			},
			{
				"box": {
					"id": "toggle",
					"maxclass": "toggle",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						"int"
					],
					"patching_rect": [
						40.0,
						290.0,
						24.0,
						24.0
					]
				}
			},
			{
				"box": {
					"id": "note",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						40.0,
						330.0,
						540.0,
						40.0
					],
					"text": "A positive @framerate emits continuously; raise the toggle to monitor. Use @framerate 0 for change-triggered sending.\nArt-Net is intended for DMX rates around 44 Hz or lower.",
					"fontsize": 12.0
				}
			}
		],
		"lines": [
			{
				"patchline": {
					"source": [
						"values",
						0
					],
					"destination": [
						"send",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"bang",
						0
					],
					"destination": [
						"send",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"receive",
						0
					],
					"destination": [
						"gate",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"gate",
						0
					],
					"destination": [
						"slice",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"slice",
						0
					],
					"destination": [
						"print",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"toggle",
						0
					],
					"destination": [
						"gate",
						1
					]
				}
			}
		],
		"dependency_cache": [
			{
				"name": "flux.artnet.send.mxo",
				"type": "iLaX"
			},
			{
				"name": "flux.artnet.receive.mxo",
				"type": "iLaX"
			}
		],
		"autosave": 0,
		"styles": [
			{
				"name": "tap",
				"default": {
					"fontname": [
						"Lato Light"
					]
				},
				"parentstyle": "",
				"multi": 0
			}
		],
		"bgfillcolor_type": "gradient",
		"bgfillcolor_color1": [
			0.376471,
			0.384314,
			0.4,
			1.0
		],
		"bgfillcolor_color2": [
			0.290196,
			0.309804,
			0.301961,
			1.0
		],
		"bgfillcolor_color": [
			0.290196,
			0.309804,
			0.301961,
			1.0
		],
		"bgfillcolor_angle": 270.0,
		"bgfillcolor_proportion": 0.39
	}
}
