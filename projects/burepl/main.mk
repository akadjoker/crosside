{
  "Name": "burepl",
  "BuildCache": "burepl_core",
  "Release": "default.json",

  "Modules": ["miniz", "bu"],
  "Src": ["src/main.cpp"],
  "Include": ["src"],

  "Main": {
    "CPP": [],
    "CC": [],
    "LD": []
  },
  "Desktop": {
    "CPP": [],
    "CC": [],
    "LD": [],
    "CONTENT_ROOT": "."
  },
  "Android": {
    "PACKAGE": "com.djokersoft.burepl",
    "ACTIVITY": "android.app.NativeActivity",
    "LABEL": "Bu REPL",
    "CPP": ["-fexceptions", "-frtti"],
    "CC": [],
    "LD": [],
    "CONTENT_ROOT": "."
  },
  "Web": {
    "SHELL": "../bugame/shell.html",
    "CPP": [],
    "CC": [],
    "LD": [],
    "CONTENT_ROOT": "."
  }
}
