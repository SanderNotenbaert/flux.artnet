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
			540.0
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
					"text": "flux.artnet.send \u2014 Art-Net DMX sender",
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
					"text": "Sends DMX channel values as Art-Net ArtDmx packets. Channels are zero-based (0-511).\n@universes is an ordered list of Port-Addresses; message universe indices select entries in that list.\nIntegers are DMX 0-255; normalized floats 0.0-1.0 are also accepted (clipped and rounded).\nLeft outlet: bang for every transmitted universe frame. Right outlet: error or warning <message>.\nLists longer than the remaining @universes capacity are truncated with a warning.\nblackout 1 sends zeros on every universe; blackout 0 restores the retained frames.\nErrors and warnings print to the Max console by default; @quiet 1 suppresses the console copy.",
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
					"text": "1. Basic loopback: send a list of ints (0-255) or normalized floats (0.0-1.0), then bang.\nThe receiver shows the first four channels and the list length.",
					"fontsize": 14.0
				}
			},
			{
				"box": {
					"id": "sender",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"bang",
						""
					],
					"patching_rect": [
						40.0,
						155.0,
						390.0,
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
						190.0,
						200.0,
						23.0
					],
					"text": "0 64 128 255"
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
						190.0,
						24.0,
						24.0
					]
				}
			},
			{
				"box": {
					"id": "print-sent",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 0,
					"outlettype": [],
					"patching_rect": [
						450.0,
						155.0,
						140.0,
						23.0
					],
					"text": "print frame-sent"
				}
			},
			{
				"box": {
					"id": "print-status",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 0,
					"outlettype": [],
					"patching_rect": [
						450.0,
						190.0,
						140.0,
						23.0
					],
					"text": "print status"
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
						250.0,
						230.0,
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
						300.0,
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
						335.0,
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
						300.0,
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
						335.0,
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
						400.0,
						250.0,
						280.0,
						40.0
					],
					"text": "No @host means broadcast. Use @host 127.0.0.1 for loopback testing.",
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
						390.0,
						660.0,
						25.0
					],
					"text": "2. Partial updates and silent set: channel 510 spills into the next universe.",
					"fontsize": 14.0
				}
			},
			{
				"box": {
					"id": "sender2",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"bang",
						""
					],
					"patching_rect": [
						40.0,
						425.0,
						390.0,
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
						460.0,
						190.0,
						23.0
					],
					"text": "channel 510 10 20 30 40"
				}
			},
			{
				"box": {
					"id": "silent",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						250.0,
						460.0,
						200.0,
						23.0
					],
					"text": "set universe 1 channel 0 255"
				}
			},
			{
				"box": {
					"id": "bang2",
					"maxclass": "button",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						"bang"
					],
					"patching_rect": [
						470.0,
						460.0,
						24.0,
						24.0
					]
				}
			},
			{
				"box": {
					"id": "ex2-note",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						40.0,
						495.0,
						500.0,
						25.0
					],
					"text": "set updates the buffer without sending. bang flushes every configured universe immediately.",
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
						"sender",
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
						"sender",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"sender",
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
						"sender",
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
						"partial",
						0
					],
					"destination": [
						"sender2",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"silent",
						0
					],
					"destination": [
						"sender2",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"bang2",
						0
					],
					"destination": [
						"sender2",
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
