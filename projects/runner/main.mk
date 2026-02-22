{
  "Name": "runner",
  "BuildCache": "runner_core",
  "Release": "default.json",

  "Modules": ["raylib", "box2d", "poly2tri", "miniz", "bu", "graphics"],
  "Src": ["../bugame/src/@.cpp"],
  "Include": ["../bugame/src"],

  "Main": {
    "CPP": ["-DBU_RUNNER_ONLY=1"],
    "CC": [],
    "LD": []
  },
  "Desktop": {
    "CPP": [],
    "CC": [],
    "LD": [],
    "CONTENT_ROOT": "../bugame"
  },
  "Android": {
    "PACKAGE": "com.djokersoft.burunner",
    "ACTIVITY": "android.app.NativeActivity",
    "LABEL": "Bu Runner",
    "CPP": ["-fexceptions", "-frtti"],
    "CC": [],
    "LD": [],
    "CONTENT_ROOT": "../bugame"
  },
  "Web": {
    "SHELL": "../bugame/shell.html",
    "CPP": [],
    "CC": [],
    "LD": [],
    "CONTENT_ROOT": "../bugame"
  }
}
