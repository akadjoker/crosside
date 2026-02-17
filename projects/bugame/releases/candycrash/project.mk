{
  "Path": "../..",
  "Name": "bucandycrash",
  "BuildCache": "bugame_core",
  "Modules": ["raylib", "box2d", "poly2tri", "miniz", "bu", "graphics"],
  "Src": ["src/@.cpp"],
  "Include": ["src"],
  "Main": { "CPP": [], "CC": [], "LD": [] },
  "Desktop": { "CPP": [], "CC": [], "LD": [] },
  "Android": {
    "PACKAGE": "com.djokersoft.bucandycrash",
    "ACTIVITY": "android.app.NativeActivity",
    "LABEL": "CandyCrash",
    "CONTENT_ROOT": "releases/candycrash",
    "CPP": ["-fexceptions", "-frtti"],
    "CC": [],
    "LD": []
  },
  "Web": {
    "SHELL": "shell.html",
    "CONTENT_ROOT": "releases/candycrash",
    "CPP": [],
    "CC": [],
    "LD": []
  }
}
