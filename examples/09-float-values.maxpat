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
			400.0
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
					"text": "Normalized float values",
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
					"text": "flux.artnet.send accepts integers 0-255 or normalized floats 0.0-1.0.\nFloats are clipped to 0-1 and rounded to the nearest DMX step. The receiver has @floatoutput 1, so it emits 0.0-1.0 floats.",
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
						115.0,
						340.0,
						23.0
					],
					"text": "flux.artnet.send @host 127.0.0.1 @universes 0"
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
						150.0,
						200.0,
						23.0
					],
					"text": "0.0 0.25 0.5 0.75 1.0"
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
						260.0,
						150.0,
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
						215.0,
						340.0,
						23.0
					],
					"text": "flux.artnet.receive @universes 0 @floatoutput 1"
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
						40.0,
						260.0,
						100.0,
						23.0
					],
					"text": "zl slice 5"
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
						40.0,
						295.0,
						180.0,
						23.0
					],
					"text": "print normalized-first-5"
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
						340.0,
						560.0,
						40.0
					],
					"text": "The same values could be written as 0 64 128 191 255.\nWith @floatoutput 0 (default), the receiver would emit integers 0-255.",
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
						"send",
						0
					],
					"destination": [
						"print-sent",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"send",
						1
					],
					"destination": [
						"print-status",
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
