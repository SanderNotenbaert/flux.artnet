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
			660.0,
			360.0
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
						640.0,
						30.0
					],
					"text": "Multiple receive outlets",
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
						640.0,
						40.0
					],
					"text": "@universes 1 2 4 creates three outlets in list order. Outlet 0 is leftmost and carries Port-Address 1.\nEach outlet emits exactly 512 values; the universe number is not prepended.",
					"fontsize": 12.0
				}
			},
			{
				"box": {
					"id": "receive",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 3,
					"outlettype": [
						"list",
						"list",
						"list"
					],
					"patching_rect": [
						40.0,
						110.0,
						260.0,
						23.0
					],
					"text": "flux.artnet.receive @universes 1 2 4"
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
						155.0,
						70.0,
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
						130.0,
						155.0,
						70.0,
						23.0
					],
					"text": "zl len"
				}
			},
			{
				"box": {
					"id": "len2",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						220.0,
						155.0,
						70.0,
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
						190.0,
						130.0,
						23.0
					],
					"text": "print outlet-0-pa-1"
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
						130.0,
						190.0,
						130.0,
						23.0
					],
					"text": "print outlet-1-pa-2"
				}
			},
			{
				"box": {
					"id": "print2",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 0,
					"outlettype": [],
					"patching_rect": [
						220.0,
						190.0,
						130.0,
						23.0
					],
					"text": "print outlet-2-pa-4"
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
						225.0,
						110.0,
						23.0
					],
					"text": "zl slice 4"
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
						260.0,
						150.0,
						23.0
					],
					"text": "print outlet-0-first-4"
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
						305.0,
						520.0,
						30.0
					],
					"text": "To change universes, recreate the object. Runtime @universes changes are rejected to keep outlets consistent.",
					"fontsize": 12.0
				}
			}
		],
		"lines": [
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
						"receive",
						2
					],
					"destination": [
						"len2",
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
						"len2",
						0
					],
					"destination": [
						"print2",
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
						"slice0",
						0
					],
					"destination": [
						"print-slice0",
						0
					]
				}
			}
		],
		"dependency_cache": [
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
