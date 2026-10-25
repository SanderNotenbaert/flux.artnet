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
			650.0,
			430.0
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
						650.0,
						30.0
					],
					"text": "Partial updates, channel offsets and spill-over",
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
						650.0,
						40.0
					],
					"text": "With @universes 1 2, a write at channel 510 spills into the next configured universe.\nThe receiver has two outlets, one per configured Port-Address.",
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
						340.0,
						23.0
					],
					"text": "flux.artnet.send @host 127.0.0.1 @universes 1 2"
				}
			},
			{
				"box": {
					"id": "partial",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						40.0,
						145.0,
						180.0,
						23.0
					],
					"text": "channel 510 10 20 30 40"
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
						240.0,
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
					"numoutlets": 2,
					"outlettype": [
						"list",
						"list"
					],
					"patching_rect": [
						40.0,
						210.0,
						230.0,
						23.0
					],
					"text": "flux.artnet.receive @universes 1 2"
				}
			},
			{
				"box": {
					"id": "len0",
					"maxclass": "newobj",
					"numinlets": 1,
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
					"text": "zl len"
				}
			},
			{
				"box": {
					"id": "len1",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						140.0,
						255.0,
						80.0,
						23.0
					],
					"text": "zl len"
				}
			},
			{
				"box": {
					"id": "print0",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 0,
					"outlettype": [],
					"patching_rect": [
						40.0,
						290.0,
						130.0,
						23.0
					],
					"text": "print universe-1"
				}
			},
			{
				"box": {
					"id": "print1",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 0,
					"outlettype": [],
					"patching_rect": [
						140.0,
						290.0,
						130.0,
						23.0
					],
					"text": "print universe-2"
				}
			},
			{
				"box": {
					"id": "slice0",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						40.0,
						325.0,
						130.0,
						23.0
					],
					"text": "zl mth 510"
				}
			},
			{
				"box": {
					"id": "slice1",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						240.0,
						325.0,
						100.0,
						23.0
					],
					"text": "zl slice 2"
				}
			},
			{
				"box": {
					"id": "print-slice0",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 0,
					"outlettype": [],
					"patching_rect": [
						40.0,
						360.0,
						170.0,
						23.0
					],
					"text": "print universe-1-510"
				}
			},
			{
				"box": {
					"id": "print-slice1",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 0,
					"outlettype": [],
					"patching_rect": [
						240.0,
						360.0,
						150.0,
						23.0
					],
					"text": "print universe-2-0-1"
				}
			}
		],
		"lines": [
			{
				"patchline": {
					"source": [
						"partial",
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
						"len0",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"receive",
						1
					],
					"destination": [
						"len1",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"len0",
						0
					],
					"destination": [
						"print0",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"len1",
						0
					],
					"destination": [
						"print1",
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
						"slice0",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"receive",
						1
					],
					"destination": [
						"slice1",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"slice0",
						0
					],
					"destination": [
						"print-slice0",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"slice1",
						0
					],
					"destination": [
						"print-slice1",
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
