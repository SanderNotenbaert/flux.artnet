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
			420.0
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
					"text": "Unicast and broadcast",
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
					"text": "With no @host, the sender broadcasts to every active IPv4 interface.\nWith @host set, it unicasts to that address. Clear the host with @host \"\" to return to broadcast.",
					"fontsize": 12.0
				}
			},
			{
				"box": {
					"id": "unicast-label",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						40.0,
						110.0,
						300.0,
						20.0
					],
					"text": "Unicast to this Mac (loopback):",
					"fontsize": 14.0
				}
			},
			{
				"box": {
					"id": "unicast",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"bang",
						""
					],
					"patching_rect": [
						40.0,
						140.0,
						340.0,
						23.0
					],
					"text": "flux.artnet.send @host 127.0.0.1 @universes 0"
				}
			},
			{
				"box": {
					"id": "unicast-values",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						40.0,
						175.0,
						140.0,
						23.0
					],
					"text": "1 2 3 4"
				}
			},
			{
				"box": {
					"id": "unicast-bang",
					"maxclass": "button",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						"bang"
					],
					"patching_rect": [
						200.0,
						175.0,
						24.0,
						24.0
					]
				}
			},
			{
				"box": {
					"id": "broadcast-label",
					"maxclass": "comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						40.0,
						230.0,
						300.0,
						20.0
					],
					"text": "Broadcast (no host):",
					"fontsize": 14.0
				}
			},
			{
				"box": {
					"id": "broadcast",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 2,
					"outlettype": [
						"bang",
						""
					],
					"patching_rect": [
						40.0,
						260.0,
						300.0,
						23.0
					],
					"text": "flux.artnet.send @universes 0"
				}
			},
			{
				"box": {
					"id": "broadcast-values",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"outlettype": [
						""
					],
					"patching_rect": [
						40.0,
						295.0,
						140.0,
						23.0
					],
					"text": "5 6 7 8"
				}
			},
			{
				"box": {
					"id": "broadcast-bang",
					"maxclass": "button",
					"numinlets": 1,
					"numoutlets": 1,
					"outlettype": [
						"bang"
					],
					"patching_rect": [
						200.0,
						295.0,
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
						350.0,
						560.0,
						45.0
					],
					"text": "Broadcast may not cross subnets or VLANs. Use unicast to a node IP when broadcast does not reach it.\nA broadcast sender is not guaranteed to reach a receiver on the same machine; use loopback for local tests.",
					"fontsize": 12.0
				}
			}
		],
		"lines": [
			{
				"patchline": {
					"source": [
						"unicast-values",
						0
					],
					"destination": [
						"unicast",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"unicast-bang",
						0
					],
					"destination": [
						"unicast",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"broadcast-values",
						0
					],
					"destination": [
						"broadcast",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"broadcast-bang",
						0
					],
					"destination": [
						"broadcast",
						0
					]
				}
			}
		],
		"dependency_cache": [
			{
				"name": "flux.artnet.send.mxo",
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
