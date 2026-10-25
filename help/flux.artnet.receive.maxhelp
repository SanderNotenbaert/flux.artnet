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
			720.0,
			650.0
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
						660.0,
						30.0
					],
					"text": "flux.artnet.receive \u2014 Art-Net DMX receiver",
					"fontsize": 18.0
				}
			},
			{
				"box": {
					"id": "intro",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						20.0,
						55.0,
						660.0,
						40.0
					],
					"text": "Receives Art-Net ArtDmx packets. One outlet is created per @universes entry, in list order.\nEach outlet emits a plain list of exactly 512 zero-based channel values.\nErrors print to the Max console by default; @quiet 1 suppresses them.",
					"fontsize": 12.0
				}
			},
			{
				"box": {
					"id": "ex1",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						20.0,
						110.0,
						660.0,
						35.0
					],
					"text": "1. Single universe: connect an Art-Net sender to the same port and watch the list length and first channels.",
					"fontsize": 14.0
				}
			},
			{
				"box": {
					"id": "receiver",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						"list"
					],
					"patching_rect": [
						40.0,
						155.0,
						300.0,
						23.0
					],
					"text": "flux.artnet.receive @universes 0"
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
						205.0,
						130.0,
						23.0
					],
					"text": "zl slice 4"
				}
			},
			{
				"box": {
					"id": "print-slice",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 0,
					"outlettype": [],
					"patching_rect": [
						40.0,
						240.0,
						120.0,
						23.0
					],
					"text": "print first-4"
				}
			},
			{
				"box": {
					"id": "length",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						260.0,
						205.0,
						90.0,
						23.0
					],
					"text": "zl len"
				}
			},
			{
				"box": {
					"id": "print-length",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 0,
					"outlettype": [],
					"patching_rect": [
						260.0,
						240.0,
						110.0,
						23.0
					],
					"text": "print length"
				}
			},
			{
				"box": {
					"id": "ex1-note",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						40.0,
						275.0,
						560.0,
						40.0
					],
					"text": "Default @framerate 0 emits only when the stored 512-channel frame changes.\nbang emits the current frame immediately, including the initial zero frame.\nSet @floatoutput 1 to emit normalized floats 0.0-1.0 instead of integers.",
					"fontsize": 12.0
				}
			},
			{
				"box": {
					"id": "ex2",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						20.0,
						340.0,
						660.0,
						35.0
					],
					"text": "2. Multiple universes and outlets: outlet 0 = Port-Address 1, outlet 1 = Port-Address 2, outlet 2 = Port-Address 4.",
					"fontsize": 14.0
				}
			},
			{
				"box": {
					"id": "receiver2",
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
						385.0,
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
						435.0,
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
						435.0,
						80.0,
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
						240.0,
						435.0,
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
						470.0,
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
						470.0,
						130.0,
						23.0
					],
					"text": "print universe-2"
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
						240.0,
						470.0,
						130.0,
						23.0
					],
					"text": "print universe-4"
				}
			},
			{
				"box": {
					"id": "ex2-note",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						340.0,
						400.0,
						340.0,
						50.0
					],
					"text": "The universe index is a position in @universes, not the raw Port-Address.\nOutlets are typed as list; outlet i corresponds to @universes[i].\n@universes is creation-time only for the receiver: recreate the object to change outlets.",
					"fontsize": 12.0
				}
			},
			{
				"box": {
					"id": "ex3",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						20.0,
						520.0,
						660.0,
						25.0
					],
					"text": "3. Fixed-rate mode: set @framerate to a positive value to emit the latest frame for every universe at that rate.",
					"fontsize": 14.0
				}
			},
			{
				"box": {
					"id": "receiver3",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						"list"
					],
					"patching_rect": [
						40.0,
						555.0,
						430.0,
						23.0
					],
					"text": "flux.artnet.receive @universes 0 @framerate 30"
				}
			},
			{
				"box": {
					"id": "gate3",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						40.0,
						595.0,
						80.0,
						23.0
					],
					"text": "gate 1"
				}
			},
			{
				"box": {
					"id": "len3",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						140.0,
						595.0,
						80.0,
						23.0
					],
					"text": "zl len"
				}
			},
			{
				"box": {
					"id": "print3",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 0,
					"outlettype": [],
					"patching_rect": [
						140.0,
						630.0,
						130.0,
						23.0
					],
					"text": "print rate-length"
				}
			},
			{
				"box": {
					"id": "toggle3",
					"maxclass": "toggle",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						"int"
					],
					"patching_rect": [
						40.0,
						630.0,
						24.0,
						24.0
					]
				}
			},
			{
				"box": {
					"id": "ex3-note",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						240.0,
						620.0,
						360.0,
						30.0
					],
					"text": "A positive @framerate emits continuously. Raise the toggle to pass the output to the length monitor.",
					"fontsize": 12.0
				}
			}
		],
		"lines": [
			{
				"patchline": {
					"source": [
						"receiver",
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
						"receiver",
						0
					],
					"destination": [
						"length",
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
						"print-slice",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"length",
						0
					],
					"destination": [
						"print-length",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"receiver2",
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
						"receiver2",
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
						"receiver2",
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
						"receiver3",
						0
					],
					"destination": [
						"gate3",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"gate3",
						0
					],
					"destination": [
						"len3",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"len3",
						0
					],
					"destination": [
						"print3",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"toggle3",
						0
					],
					"destination": [
						"gate3",
						1
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
