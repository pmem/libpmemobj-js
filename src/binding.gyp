{
		"targets": [
				{
						"target_name": "jspmdk",
						
						"sources": [
								"jspmdk.cc", 
								"persistentobjectpool.cc",
								"persistentobject.cc",
								"persistentarraybuffer.cc",
								"internal/memorymanager.cc",
								"internal/pmdict.cc",
								"internal/pmarray.cc",
								"internal/pmobjectpool.cc",
								"internal/pmobject.cc",
								"internal/pmarraybuffer.cc",
						],
						
						"include_dirs": [
								"<!@(node -p \"require('node-addon-api').include\")"
						],
						
						"dependencies": [
								"<!(node -p \"require('node-addon-api').gyp\")"
						],
						
						"libraries": [
								"-lpmem",
								"-lpmemobj",
								"-lpthread",
								"-lgcov"
						],
						
						"cflags_cc": [
								"-Wno-return-type",
								"-fexceptions",
								"-O3",
								"-fdata-sections",
								"-ffunction-sections",
								"-fno-strict-overflow",
								"-fno-delete-null-pointer-checks",
								"-fwrapv"
						],
						'link_settings': {
							'ldflags': [
								'-Wl,--gc-sections',
							]
						},
						"defines": [

						]
				}
		]
}
