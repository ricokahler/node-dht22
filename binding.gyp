{
    "targets": [
        {
            "target_name": "node-dht22",
            "conditions": [
                ["OS=='linux'", {
                    "sources": [
                        "src/dht22.cc",
                        "src/dht22.cc",
                        "src/main.cc"
                    ],
                    "include_dirs": [
                        "<!(node -e \"require('nan')\")"
                    ],
                    "libraries": [
                        "-lgpiod"
                    ],
                }]
            ]
        }
    ],
}
