#!/usr/bin/env python



from PyQt5 import QtCore
from PyQt5.QtCore import QCoreApplication, QFile, QMetaObject,QFileInfo, QRect, QRegExp, QSize, QStringListModel, Qt, QTextCodec, pyqtSlot,QThread,QRunnable, Qt, QThreadPool,pyqtSignal,QObject

from PyQt5.QtGui import (QColor, QCursor, QFont, QFontDatabase, QFontInfo, QIcon, QKeySequence, QPainter,
        QPixmap, QSyntaxHighlighter, QTextBlockFormat, QTextCharFormat, QTextCursor,QFontMetrics,
        QTextDocumentWriter, QTextFormat, QTextListFormat)
from PyQt5.QtWidgets import (QAction, QActionGroup, QApplication, QColorDialog, QProgressBar,QStatusBar,QDesktopWidget,QRadioButton,
        QComboBox, QCompleter, QFileDialog, QFontComboBox, QGroupBox, QHBoxLayout, QListWidget, QMainWindow, QMenu, QMessageBox, QPlainTextEdit, QPushButton, QSizePolicy,
        QTextEdit, QToolBar,QSpacerItem,QVBoxLayout)
from PyQt5.QtPrintSupport import QPrintDialog, QPrinter, QPrintPreviewDialog

from PyQt5.QtCore import QDate, Qt
from PyQt5.QtGui import (QFont, QTextCharFormat, QTextCursor, QTextFrameFormat,
        QTextLength, QTextTableFormat)
from PyQt5.QtWidgets import (QApplication, QCheckBox, QDialog, QWidget,QDockWidget,
        QDialogButtonBox, QGridLayout, QLabel, QLineEdit, QMainWindow,
        QMessageBox, QMenu, QTableWidget, QTableWidgetItem, QTabWidget,
        QTextEdit)
from PyQt5.QtPrintSupport import QAbstractPrintDialog, QPrintDialog, QPrinter
from PyQt5.Qsci import *
import sys
import os
import json

from subprocess import STDOUT, PIPE
import subprocess
import shlex
import argparse
import shutil
import time
import json
import re
import subprocess
from subprocess import check_output, CalledProcessError, STDOUT
from datetime import datetime
import logging
import random
import sys
import time
import http.server
import socketserver
import webbrowser
from urllib.parse import quote


from ide_utils.fs_utils import (
    cleanString,
    createFolderTree,
    createPath,
    getFoldersTree,
    getParentDir,
)
from ide_utils.android_pipeline import (
    android_compile_java_native as _android_compile_java_native,
    android_install_app as _android_install_app,
    android_remove as _android_remove,
    build_native_manifest,
)
from ide_utils.android_native_build import (
    android_build as _android_build,
    android_builder as _android_builder,
    android_compile as _android_compile,
)
from ide_utils.native_build import (
    desktop_build as _desktop_build,
    desktop_compile as _desktop_compile,
    web_build as _web_build,
    web_compile as _web_compile,
)
from ide_utils.process_utils import runProcess
from ide_utils.toolchain import resolve_toolchain_paths


nativeJavaManifest="""<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="@apppkg@"     android:versionCode="1" android:versionName="1.0">
            <uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE"/>
            <uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE"/>
            <uses-sdk  android:compileSdkVersion="30"     android:minSdkVersion="23"  android:targetSdkVersion="30" />
           <uses-feature android:glEsVersion="0x00020000" android:required="true" />
            <application android:allowBackup="false" android:label="@applbl@" android:icon="@mipmap/ic_icon" >

           <activity android:name="@appactv@"
             android:theme="@android:style/Theme.NoTitleBar.Fullscreen"
             android:configChanges="orientation|keyboardHidden|screenSize"
             android:screenOrientation="landscape" android:launchMode="singleTask"
             android:clearTaskOnLaunch="true">
            <meta-data android:name="android.app.lib_name" android:value="main" />
                               
             <intent-filter>
                  <action android:name="android.intent.action.MAIN" />
                   <category android:name="android.intent.category.LAUNCHER" />
         </intent-filter>
           </activity>
           </application>
</manifest>"""

nativeManifest="""<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="@apppkg@"
          android:versionCode="1"
          android:versionName="1.0">

           <uses-sdk  android:compileSdkVersion="30"     android:minSdkVersion="16"  android:targetSdkVersion="23" />

  <application
      android:allowBackup="false"
      android:fullBackupContent="false"
      android:icon="@mipmap/ic_launcher"
      android:label="@applbl@"
      android:hasCode="false">


    <activity android:name="@appact@"
              android:label="@applbl@"
              android:configChanges="orientation|keyboardHidden|screenSize"
             android:screenOrientation="landscape" android:launchMode="singleTask"
             android:clearTaskOnLaunch="true">

      <meta-data android:name="android.app.lib_name"
                 android:value="@appLIBNAME@" />
      <intent-filter>
        <action android:name="android.intent.action.MAIN" />
        <category android:name="android.intent.category.LAUNCHER" />
      </intent-filter>
    </activity>
  </application>

</manifest>"""
NATIVE_MANIFEST_TEMPLATE = nativeManifest


javaNative="""package @apppkg@; 

public class @appactv@ extends android.app.NativeActivity {
  static {
  System.loadLibrary("main");
 }
}"""
OSP=os.path.sep

_toolchain = resolve_toolchain_paths(
    config_path=os.path.join(os.getcwd(), "config.json"),
    default_sdk="/home/djoker/android/android-sdk",
    default_ndk="/home/djoker/android/android-ndk-r27d",
    default_java="/usr/lib/jvm/java-11-openjdk-amd64",
    preferred_build_tools="30.0.2",
    preferred_platform="android-31",
)
ANDROID_SDK = _toolchain.ANDROID_SDK
ANDROID_NDK = _toolchain.ANDROID_NDK
AAPT = _toolchain.AAPT
DX = _toolchain.DX
DX8 = _toolchain.DX8
ZIPALIGN = _toolchain.ZIPALIGN
APKSIGNER = _toolchain.APKSIGNER
PLATFORM = _toolchain.PLATFORM
JAVA_SDK = _toolchain.JAVA_SDK
JAVA_LIB_RT = _toolchain.JAVA_LIB_RT
JAVAFX = _toolchain.JAVAFX


KEYTOOL= JAVA_SDK+'/bin/keytool'

EMCC ="/media/projectos/projects/emsdk/upstream/emscripten/emcc"
EMCPP ="/media/projectos/projects/emsdk/upstream/emscripten/em++"
EMAR ="/media/projectos/projects/emsdk/upstream/emscripten/emar"

PORT = 8080
DIRECTORY = "/media/projectos/projects/python/crosside"


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=DIRECTORY, **kwargs)
    def do_GET(self):
        print("get",self.requestline)
        
        self.send_response(200)
        self.send_header('Content-type','text/html')
        self.end_headers()

rsrcPath = os.getcwd()+os.path.sep+"res"
templatesPath = os.getcwd()+os.path.sep+"Templates"
modulesPath = os.getcwd()+os.path.sep+"modules"
projectsPath = os.getcwd()+os.path.sep+"projects"

#debugs

showSRCLoad=False

isLinux=True
    

def sanitize_android_package_name(package_name, fallback="com.djokersoft.game"):
    value = (package_name or "").strip().replace("/", ".")
    value = re.sub(r"[^A-Za-z0-9_.]", "", value)
    value = re.sub(r"\.+", ".", value).strip(".")
    parts = [part for part in value.split(".") if part]

    clean_parts = []
    for part in parts:
        cleaned = re.sub(r"[^A-Za-z0-9_]", "", part)
        if not cleaned:
            continue
        if cleaned[0].isdigit():
            cleaned = "p" + cleaned
        clean_parts.append(cleaned)

    if len(clean_parts) < 2:
        return fallback
    return ".".join(clean_parts)


def ensure_android_native_template(parent, main_root, app_name, android_pack, android_label, android_activity):
    osp = os.path.sep
    out_folder = main_root + osp + "Android" + osp + app_name + osp
    createFolderTree(out_folder)

    res_root = out_folder + "res"
    createFolderTree(res_root)

    icon_src = templatesPath + osp + "Android" + osp + "Res" + osp + "mipmap-hdpi" + osp + "ic_launcher.png"
    for bucket in ("mipmap-hdpi", "mipmap-mdpi", "mipmap-xhdpi", "mipmap-xxhdpi"):
        bucket_dir = res_root + osp + bucket
        createFolderTree(bucket_dir)
        icon_dst = bucket_dir + osp + "ic_launcher.png"
        if os.path.exists(icon_src) and not os.path.exists(icon_dst):
            try:
                shutil.copy(icon_src, bucket_dir)
            except Exception as e:
                parent.trace("Warn copy android icon: ", type(e).__name__, " ", e)

    android_pack = sanitize_android_package_name(android_pack)
    if not android_label or not android_label.strip():
        android_label = app_name
    if not android_activity or not android_activity.strip():
        android_activity = "android.app.NativeActivity"

    if android_activity.startswith("."):
        android_activity = android_pack + android_activity
    elif "." not in android_activity:
        android_activity = android_pack + "." + android_activity

    manifest_file = out_folder + "AndroidManifest.xml"
    if not os.path.exists(manifest_file):
        manifest_data = build_native_manifest(
            NATIVE_MANIFEST_TEMPLATE,
            android_pack,
            android_label,
            android_activity,
            app_name,
        )
        with open(manifest_file, "w", encoding="utf-8") as text_file:
            text_file.write(manifest_data)

    return android_pack, android_label, android_activity

    
def DesktopCompile(parent,folderRoot ,name, srcs ,CARGS,CPPARGS, fullBuild=False):
    return _desktop_compile(parent, folderRoot, name, srcs, CARGS, CPPARGS, fullBuild)
    
 
def DesktopBuild(parent,folderRoot ,appName, useCPP,LDARGS , BuildType=0):
    return _desktop_build(parent, folderRoot, appName, useCPP, LDARGS, BuildType)
    
    
def WebCompile(parent,folderRoot ,name, srcs ,CARGS,CPPARGS, fullBuild=False):
    return _web_compile(parent, folderRoot, name, srcs, CARGS, CPPARGS, EMCC, EMCPP, fullBuild)
    
       
def WebBuild(parent, folderRoot ,appName, useCPP,LDARGS , isStatic=True):
    return _web_build(parent, folderRoot, appName, useCPP, LDARGS, EMCC, EMCPP, EMAR, isStatic)


def androidRemove(parent,pack):
    return _android_remove(parent, ANDROID_SDK, pack)
    
def androidInstallApp(parent,appSigned):
    return _android_install_app(parent, ANDROID_SDK, appSigned)



def androidCompileJavaNative(parent,mainRoot,appType,appName,ANDROID_PACK,ANDROID_LABEL,ANDROID_ACTIVITY,runApp):
    return _android_compile_java_native(
        parent,
        mainRoot,
        appType,
        appName,
        ANDROID_PACK,
        ANDROID_LABEL,
        ANDROID_ACTIVITY,
        runApp,
        android_sdk=ANDROID_SDK,
        aapt=AAPT,
        dx=DX,
        dx8=DX8,
        apksigner=APKSIGNER,
        platform=PLATFORM,
        keytool=KEYTOOL,
        native_manifest_template=NATIVE_MANIFEST_TEMPLATE,
    )




def AndroidBuilder(parent,folderRoot ,name, srcs ,CARGS,CPPARGS, LDARGS,buildType=0,arm=0,fullBuild=False):
    return _android_builder(
        parent,
        folderRoot,
        name,
        srcs,
        CARGS,
        CPPARGS,
        LDARGS,
        buildType,
        arm,
        fullBuild,
        android_ndk=ANDROID_NDK,
    )

def AndroidCompile(parent,folderRoot ,name, srcs ,CARGS,CPPARGS,arm=0,fullBuild=False):
    return _android_compile(
        parent,
        folderRoot,
        name,
        srcs,
        CARGS,
        CPPARGS,
        arm,
        fullBuild,
        android_ndk=ANDROID_NDK,
    )


def AndroidBuild(parent,folderRoot ,name,useCPP, LDARGS,buildType=0,arm=0):
    return _android_build(
        parent,
        folderRoot,
        name,
        useCPP,
        LDARGS,
        buildType,
        arm,
        android_ndk=ANDROID_NDK,
    )


def resolveWebExport(folderRoot, appName):
    base = folderRoot + os.path.sep + "Web" + os.path.sep
    candidates = [
        base + appName + ".html",
        base + appName + os.path.sep + appName + ".html",
    ]
    for export in candidates:
        if os.path.exists(export):
            return export
    return None


def resolveWebServeInfo(exportPath):
    workspace_root = os.getcwd()
    serve_dir = workspace_root
    rel = os.path.relpath(exportPath, workspace_root)
    if rel.startswith(".."):
        serve_dir = os.path.dirname(exportPath)
        rel = os.path.basename(exportPath)
    url_path = quote(rel.replace(os.path.sep, "/"), safe="/")
    url = "http://127.0.0.1:" + str(PORT) + "/" + url_path
    return serve_dir, url


          
   




    
class ConsoleText(QPlainTextEdit):
    def __init__(self,  parent=None):
        super(ConsoleText, self).__init__(parent)
        
        self.setReadOnly(True)
        
    # def contextMenuEvent(self, event):
    #     contextMenu = QMenu(self)
    #     clrAct = contextMenu.addAction("Clear")
    #     action = contextMenu.exec_(self.mapToGlobal(event.pos()))
    #     if action == clrAct:
    #         self.clear()
         





class Args:
    def __init__(self):
        self.ARGCPP=[]
        self.ARGCC=[]
        self.ARGLD=[]
    def addCPP(self, value):
        if not value in self.ARGCPP:
            self.ARGCPP.append(value)
    def addCC(self, value):
        if not value in self.ARGCC:
            self.ARGCC.append(value)
    def addLD(self, value):
        if not value in self.ARGLD:
            self.ARGLD.append(value)
    def clear(self):
        self.ARGCPP.clear()
        self.ARGCC.clear()
        self.ARGLD.clear()
        

class ArgsAndroid(Args):
    def __init__(self):
        super(ArgsAndroid, self).__init__()
        self.PACKAGE=""        
        self.ACTIVITY=""

class ArgsWeb(Args):
    def __init__(self):
        super(ArgsWeb, self).__init__()
        self.SHELL=""        
        
class ArgsDesktop(Args):
    def __init__(self):
        super(ArgsDesktop, self).__init__()
        self.SHELL=""        
        

class SrcCode:
    def __init__(self,plataform):
        self.Plataform=plataform
        self.Src=[]
        self.Include=[]
        self.ARGS=Args()
        self.ARGSDESKTOP=ArgsDesktop()
        self.ARGSANDROID=ArgsAndroid()
        self.ARGSWEB=ArgsWeb()
    


class AppProgress(QDialog):
    def __init__(self,parent):
        super(AppProgress, self).__init__(parent)
        self.resize(449, 155)
        sizePolicy = QSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(self.sizePolicy().hasHeightForWidth())
        self.setSizePolicy(sizePolicy)
        self.setWindowTitle(u"Progress")
        self.gridLayout = QGridLayout(self)
        self.gridLayout.setObjectName(u"gridLayout")
        self.progressBar = QProgressBar(self)
        self.progressBar.setObjectName(u"progressBar")
        self.progressBar.setValue(24)

        self.gridLayout.addWidget(self.progressBar, 0, 0, 1, 3)

        self.lineEdit = QLineEdit(self)
        self.lineEdit.setObjectName(u"lineEdit")
        self.lineEdit.setEnabled(False)
        self.lineEdit.setReadOnly(True)
        font = QFont()
        font.setPointSize(13)
        self.lineEdit.setFont(font)
        self.lineEdit.setAutoFillBackground(False)
        self.lineEdit.setInputMask(u"")
        self.lineEdit.setAlignment(Qt.AlignCenter)

        self.gridLayout.addWidget(self.lineEdit, 1, 0, 1, 3)

        self.horizontalSpacer = QSpacerItem(167, 20, QSizePolicy.Expanding, QSizePolicy.Minimum)

        self.gridLayout.addItem(self.horizontalSpacer, 2, 0, 1, 1)

        self.pushButton = QPushButton(self)
        self.pushButton.setText("Abort")
        self.gridLayout.addWidget(self.pushButton, 2, 1, 1, 1)
        self.horizontalSpacer_2 = QSpacerItem(166, 20, QSizePolicy.Expanding, QSizePolicy.Minimum)
        self.gridLayout.addItem(self.horizontalSpacer_2, 2, 2, 1, 1)

        self.pushButton.pressed.connect(self.abort)
        self.OnAbort=None
        self.show()
        QMetaObject.connectSlotsByName(self)    
        
    def setup(self,min,max,onClose):
        self.progressBar.setRange(min,max)
        self.progressBar.setValue(min)
        self.OnAbort=onClose
        
      
        
        
    def setProgress(self,progress,msg):
        self.progressBar.setValue(progress)
        self.lineEdit.setText(msg)
        #print("Compile ",code," ",value)


    def abort(self):
        self.OnAbort()
        self.close()
        
    def closeEvent(self, event):
        event.accept()


class Worker(QObject):
    finished = pyqtSignal()
    progress = pyqtSignal(int)
    isDone=False
    def run(self):
        try:
            with socketserver.TCPServer(("", PORT), Handler) as httpd:
                print("Serving at port", PORT)
                #httpd.RequestHandlerClass.directory=DIRECTORY
                httpd.serve_forever()
            while (not self.isDone):
                pass
            httpd.server_close()
        
        except  Exception as error:
            print(" Error  Startin Server :",error)  
        
        

class AppAndroidSetup(QDialog):
    def __init__(self,parent):
        super(AppAndroidSetup, self).__init__(parent)
        Dialog=self
        if not Dialog.objectName():
            Dialog.setObjectName(u"Dialog")
        Dialog.resize(516, 292)
        Dialog.setWindowTitle(u"Android Template")
        self.gridLayout = QGridLayout(Dialog)
        self.gridLayout.setObjectName(u"gridLayout")
        self.groupBoxAndroid = QGroupBox(Dialog)
        self.groupBoxAndroid.setObjectName(u"groupBoxAndroid")
        sizePolicy = QSizePolicy(QSizePolicy.Preferred, QSizePolicy.Fixed)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(self.groupBoxAndroid.sizePolicy().hasHeightForWidth())
        self.groupBoxAndroid.setSizePolicy(sizePolicy)
        self.groupBoxAndroid.setTitle(u"Android")
        self.gridLayout_7 = QGridLayout(self.groupBoxAndroid)
        self.gridLayout_7.setObjectName(u"gridLayout_7")
        self.groupBox_4 = QGroupBox(self.groupBoxAndroid)
        self.groupBox_4.setObjectName(u"groupBox_4")
        sizePolicy1 = QSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)
        sizePolicy1.setHorizontalStretch(0)
        sizePolicy1.setVerticalStretch(0)
        sizePolicy1.setHeightForWidth(self.groupBox_4.sizePolicy().hasHeightForWidth())
        self.groupBox_4.setSizePolicy(sizePolicy1)
        self.groupBox_4.setTitle(u"")
        self.horizontalLayout_5 = QHBoxLayout(self.groupBox_4)
        self.horizontalLayout_5.setObjectName(u"horizontalLayout_5")
        self.label_5 = QLabel(self.groupBox_4)
        self.label_5.setObjectName(u"label_5")
        self.label_5.setText(u"Activity")

        self.horizontalLayout_5.addWidget(self.label_5)

        self.lineEdit = QLineEdit(self.groupBox_4)
        self.lineEdit.setObjectName(u"lineEdit")

        self.horizontalLayout_5.addWidget(self.lineEdit)


        self.gridLayout_7.addWidget(self.groupBox_4, 0, 0, 1, 1)

        self.groupBox_6 = QGroupBox(self.groupBoxAndroid)
        self.groupBox_6.setObjectName(u"groupBox_6")
        sizePolicy1.setHeightForWidth(self.groupBox_6.sizePolicy().hasHeightForWidth())
        self.groupBox_6.setSizePolicy(sizePolicy1)
        self.groupBox_6.setTitle(u"")
        self.horizontalLayout_6 = QHBoxLayout(self.groupBox_6)
        self.horizontalLayout_6.setObjectName(u"horizontalLayout_6")
        self.label_6 = QLabel(self.groupBox_6)
        self.label_6.setObjectName(u"label_6")
        self.label_6.setText(u"Package")

        self.horizontalLayout_6.addWidget(self.label_6)

        self.lineEdit_2 = QLineEdit(self.groupBox_6)
        self.lineEdit_2.setObjectName(u"lineEdit_2")

        self.horizontalLayout_6.addWidget(self.lineEdit_2)


        self.gridLayout_7.addWidget(self.groupBox_6, 1, 0, 1, 1)

        self.groupBox_9 = QGroupBox(self.groupBoxAndroid)
        self.groupBox_9.setObjectName(u"groupBox_9")
        sizePolicy1.setHeightForWidth(self.groupBox_9.sizePolicy().hasHeightForWidth())
        self.groupBox_9.setSizePolicy(sizePolicy1)
        self.groupBox_9.setTitle(u"")
        self.horizontalLayout_7 = QHBoxLayout(self.groupBox_9)
        self.horizontalLayout_7.setObjectName(u"horizontalLayout_7")
        self.label_7 = QLabel(self.groupBox_9)
        self.label_7.setObjectName(u"label_7")
        self.label_7.setText(u" Label    ")

        self.horizontalLayout_7.addWidget(self.label_7)

        self.lineEdit_3 = QLineEdit(self.groupBox_9)
        self.lineEdit_3.setObjectName(u"lineEdit_3")

        self.horizontalLayout_7.addWidget(self.lineEdit_3)


        self.gridLayout_7.addWidget(self.groupBox_9, 2, 0, 1, 1)


        self.gridLayout.addWidget(self.groupBoxAndroid, 0, 0, 1, 1)

        self.buttonBox = QDialogButtonBox(Dialog)
        self.buttonBox.setObjectName(u"buttonBox")
        self.buttonBox.setOrientation(Qt.Vertical)
        self.buttonBox.setStandardButtons(QDialogButtonBox.Cancel|QDialogButtonBox.Ok)

        self.gridLayout.addWidget(self.buttonBox, 1, 0, 1, 1)


    
        self.buttonBox.accepted.connect(self.save)
        self.buttonBox.rejected.connect(Dialog.reject)

        QMetaObject.connectSlotsByName(Dialog)
        
        self.edit=None
        
    def save(self):
        if self.edit:
            self.edit.ACTIVITY=self.lineEdit.text().strip()
            self.edit.PACKAGE=self.lineEdit_2.text().strip()
            self.edit.LABEL=self.lineEdit_3.text().strip()
            self.close()
            
    def load(self,codeEdit):
        if codeEdit:
            self.edit=codeEdit
            self.lineEdit.setText(self.edit.ACTIVITY) 
            self.lineEdit_2.setText(self.edit.PACKAGE)
            self.lineEdit_3.setText(self.edit.LABEL)
        self.exec()
 

class AppServer(QDialog):
    def __init__(self,parent):
        super(AppServer, self).__init__(parent)
        self.resize(438, 307)
        self.setWindowTitle(u"HTTP Server")
        self.mainWindo=parent
        self.gridLayout = QGridLayout(self)
        self.gridLayout.setObjectName(u"gridLayout")
        self.plainTextEdit = QPlainTextEdit(self)
        self.plainTextEdit.setObjectName(u"plainTextEdit")
        self.plainTextEdit.setPlainText(u"")
        self.gridLayout.addWidget(self.plainTextEdit, 0, 0, 1, 1)
        self.buttonBox = QDialogButtonBox(self)
        self.buttonBox.setObjectName(u"buttonBox")
        self.buttonBox.setOrientation(Qt.Horizontal)
        self.buttonBox.setStandardButtons(QDialogButtonBox.Cancel|QDialogButtonBox.Ok)
        self.gridLayout.addWidget(self.buttonBox, 1, 0, 1, 1)
        self.buttonBox.accepted.connect(self.cancel)
        self.buttonBox.rejected.connect(self.cancel)
        self.thread = QThread()
        self.worker = Worker()
        self.thread.setTerminationEnabled(True)
        QMetaObject.connectSlotsByName(self)


    
    
    def start(self,path):
        self.path=path
        self.plainTextEdit.appendPlainText("Start web host on Port 8080")
        try:
        
            self.worker.moveToThread(self.thread)
            self.thread.started.connect(self.worker.run)
            self.worker.finished.connect(self.thread.quit)
            self.worker.finished.connect(self.worker.deleteLater)
            self.thread.finished.connect(self.thread.deleteLater)
            self.worker.progress.connect(self.reportProgress)
        
            self.thread.start()
        except  Exception as error:
            print(" Error  Startin Server :",error)  

    
        self.show()

    def reportProgress(self,index):
        print("index",index)

    def cancel(self):
        self.worker.isDone=True
        try:
            self.thread.terminate()
            self.thread.wait(100)
            
        except  Exception as error:
            print(" Error  Stoping Server :",error)  
        
        self.close()
        pass

class BuildWorker(QtCore.QThread):
    buildingComplete= pyqtSignal(str,int)
    traceMessage = pyqtSignal(str)
    def __init__(self, parent,plataform,folderRoot ,appName, useCPP, ARGSLD , buildType, arm=0):
        super(self.__class__, self).__init__(parent)
        self.parent=parent
        self.Plataform  =plataform
        self.FolderRoot =folderRoot
        self.AppName   =appName
        self.UseCPP   =  useCPP
        self.LDARGS  = ARGSLD
        self.BuildType=buildType
        self.Arm=arm

        
    def OnAbort(self):
        self.IsDone=True
        self.traceMessage.emit("Abort ...")
        self.stop()

    def run(self):
        
        if self.Plataform==0:
            if DesktopBuild(self,self.FolderRoot, self.AppName,  self.UseCPP, self.LDARGS, self.BuildType):
                self.buildingComplete.emit(self.AppName,1)
            else:
                self.buildingComplete.emit(self.AppName,0)
        
        if self.Plataform==1:
            if AndroidBuild(self,self.FolderRoot, self.AppName,  self.UseCPP, self.LDARGS, self.BuildType,self.Arm):
                self.buildingComplete.emit(self.AppName,1)
            else:
                self.buildingComplete.emit(self.AppName,0)
                
        if self.Plataform==2:
            if WebBuild(self,self.FolderRoot, self.AppName,  self.UseCPP, self.LDARGS, True):
                self.buildingComplete.emit(self.AppName,1)
            else:
                self.buildingComplete.emit(self.AppName,0)  
                
                
            
    

    def stop(self):
        self.wait()
        
        
    def setProgress(self,value,code):
        #print("Compile ",code," ",value,"%")
        self.AppProgress.setProgress(value,code)
        
    def trace(self,*args):
        result = ""
        for x in args:
            result += str(x)
        self.traceMessage.emit(result)
        
   

class RunWorker(QThread):
    def __init__(self, parent):
        super(self.__class__, self).__init__(parent)
        self.isDone=False
        self.parent=parent
        self.progress = AppProgress(self.parent)
        self.progress.setup(0,10,self.OnAbort)
        
    def OnAbort(self):
         print("abort")
         self.isDone=True
         self.stop()


    def run(self):
        for i in range(10):
            if self.isDone:
                logging.info(f"Abort")
                break
            
            self.progress.setProgress(i + 1,f"Working in thread  step {i + 1}/5")
            time.sleep(random.randint(700, 2500) / 1000)
            self.parent.MainConsole.appendPlainText(f"Working in thread  step {i + 1}/5")
            
        


    def stop(self):
        self.wait()
        
class ExecuteWorker(QThread):
    traceMessage = pyqtSignal(str)
    def __init__(self, parent,App,plataform):
        super(self.__class__, self).__init__(parent)
        self.isDone=False
        self.parent=parent
        self.app=App
        self.Plataform=plataform
        self.web_server_process=None
        
    def execute(self,cmd,args,cwd=None):
        args = [cmd] + args
        #re = subprocess.Popen(args, shell=False,close_fds=True,stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        
        process = subprocess.Popen(args,shell=False,
                           stdout=subprocess.PIPE,
                           universal_newlines=True,
                           cwd=cwd)

        while (not self.isDone):
            output = process.stdout.readline()
            self.traceMessage.emit(output.strip())
            return_code = process.poll()
            if return_code is not None:
                self.traceMessage.emit("Return : "+str(return_code))
                # Process has finished, read rest of the output 
                for output in process.stdout.readlines():
                    self.traceMessage.emit(output.strip())
                break

    def OnAbort(self):
         self.traceMessage.emit("abort")
         self.isDone=True
         self.stop()

    def stop(self):
        self.isDone=True
        if self.web_server_process and self.web_server_process.poll() is None:
            self.web_server_process.terminate()
            try:
                self.web_server_process.wait(timeout=2)
            except Exception:
                self.web_server_process.kill()
        self.web_server_process=None
        self.wait()
        
        
    def run(self):
        try:
            print("pre execute")
            
            if self.Plataform==0:
                self.parent.actionRun.isRun=True
                self.parent.actionRun.setIcon(QIcon(rsrcPath + '/process-stop.png'))
                run_cwd = os.path.dirname(self.app)
                if run_cwd:
                    self.traceMessage.emit("Run cwd: " + run_cwd)
                self.execute(self.app,[],run_cwd if run_cwd else None)
            elif self.Plataform==2:
                if self.parent.actionRun.isRun:
                    self.parent.actionRun.isRun=False
                else:
                    self.parent.actionRun.isRun=True
                    self.parent.actionRun.setIcon(QIcon(rsrcPath + '/process-stop.png'))
                    serve_dir, url = resolveWebServeInfo(self.app)
                    self.traceMessage.emit("Serve Web from " + serve_dir)
                    self.traceMessage.emit("Open " + url)
                    self.web_server_process = subprocess.Popen(
                        [sys.executable, "-m", "http.server", str(PORT), "--directory", serve_dir],
                        stdout=subprocess.DEVNULL,
                        stderr=subprocess.DEVNULL,
                    )
                    webbrowser.open_new_tab(url)
                    while not self.isDone:
                        if self.web_server_process.poll() is not None:
                            self.traceMessage.emit("Web server stopped (port busy or startup error)")
                            break
                        time.sleep(0.2)
            self.parent.actionRun.isRun=False
            self.parent.actionRun.setIcon(QIcon(rsrcPath + '/Go.png'))
            print("post execute")
            
        except Exception as e:
            #self.traceMessage.emit('Error on line {}'.format(sys.exc_info()[-1].tb_lineno) + str(type(e).__name__)+ str(e))
            print('Error on line {}'.format(sys.exc_info()[-1].tb_lineno) ,type(e).__name__,e)

        self.traceMessage.emit("Done")
        
class TestWorker(QThread):
    def __init__(self, parent):
        super(self.__class__, self).__init__(parent)
        self.isDone=False
        self.parent=parent
        self.progress = AppProgress(self.parent)
        self.progress.setup(0,10,self.OnAbort)
        
    def OnAbort(self):
         print("abort")
         self.isDone=True
         self.stop()

    def stop(self):
        self.wait()
        
    def run(self):
        for i in range(10):
            if self.isDone:
                print("Abort")
                break
            print(f"Working in thread  step {i + 1}/5")
            self.progress.setProgress(i + 1,f"Working in thread  step {i + 1}/5")
            time.sleep( 2500 / 1000)
            
        print("Done")
        
class CompileWorker(QThread):
    traceMessage = pyqtSignal(str)
    compilerComplete= pyqtSignal(str,int,list)
    progresEvent= pyqtSignal(int,str)
    def __init__(self, parent,plataform,folderRoot ,appName, ListSrc ,useCPP,CPPARGS,CCARGS, arm=0):
        super(self.__class__, self).__init__(parent)
        self.parent = parent
        self.Plataform  =plataform
        self.FolderRoot =folderRoot
        self.AppName   =appName
        self.SrcList = ListSrc
        self.UseCPP   =useCPP
        self.CPPARGS  =CPPARGS
        self.CCARGS  =CCARGS
        self.Arm = arm
        self.IsDone=False
        self.AppProgress = AppProgress(self.parent)
        #self.AppProgress.setup(0,len(self.SrcList),self.OnAbort)
        self.AppProgress.setup(0,100,self.OnAbort)
        self.progresEvent.connect(self.AppProgress.setProgress)
        
        
        
    def OnAbort(self):
        self.IsDone=True
        self.traceMessage.emit("Abort ...")
        self.stop()
        


    def run(self):
           
        if self.Plataform==0:
            DesktopCompile(self,self.FolderRoot ,self.AppName, self.SrcList ,self.CCARGS,self.CPPARGS)

        if self.Plataform==1:
            AndroidCompile(self,self.FolderRoot ,self.AppName, self.SrcList ,self.CCARGS,self.CPPARGS,self.Arm,False)
            #AndroidCompile(self,self.FolderRoot ,self.AppName, self.SrcList ,self.CCARGS,self.CPPARGS,1,False)

            
        if self.Plataform==2:
            WebCompile(self,self.FolderRoot ,self.AppName, self.SrcList ,self.CCARGS,self.CPPARGS)
            
        self.AppProgress.close()
        
        
    

    def stop(self):
        self.wait()
        
        
    def setProgress(self,value,code):
        #print("Compile ",code," ",value,"%")
        #self.AppProgress.setProgress(value,code)
        #self.AppProgress.progressBar.setValue(value)
        #self.AppProgress.lineEdit.setText(code)
        self.progresEvent.emit(value,code)
        self.traceMessage.emit(str("Compile  "+str(code) +"  ["+str(value)+"] %"))
        
        
    def trace(self,*args):
        result = ""
        for x in args:
            result += str(x)
        self.traceMessage.emit(result)
        
        

                                




class ProjectCompileWorker(QThread):
    traceMessage = pyqtSignal(str)
    compileFinished = pyqtSignal(bool, int, bool, object)

    def __init__(self, parent, project, plataform, run_after=False):
        super(self.__class__, self).__init__(parent)
        self.parent = parent
        self.project = project
        self.Plataform = plataform
        self.RunAfter = run_after

    def _traceProxy(self, *args):
        result = ""
        for x in args:
            result += str(x)
        self.traceMessage.emit(result)

    def _progressProxy(self, value, code):
        self.traceMessage.emit(str("Compile  " + str(code) + "  [" + str(value) + "] %"))

    def run(self):
        success = False
        old_trace = self.project.trace
        old_progress = self.project.setProgress
        try:
            self.project.trace = self._traceProxy
            self.project.setProgress = self._progressProxy
            success = bool(self.project.compile(self.Plataform))
        except Exception as e:
            self.traceMessage.emit(
                "Error compile project at line {} {} {}".format(
                    sys.exc_info()[-1].tb_lineno, type(e).__name__, e
                )
            )
            success = False
        finally:
            self.project.trace = old_trace
            self.project.setProgress = old_progress

        self.compileFinished.emit(success, self.Plataform, self.RunAfter, self.project)


class CppModule:
    def __init__(self,parent):
        self.Dir=""
        self.Name=""
        self.About=""
        self.Version=""
        self.Priority=0
        self.Static=True
        self.Shell=''
        self.System=[]
        self.Depends=[]
        self.MainSrc=SrcCode("Main")
        self.DesktopSrc=SrcCode("Desktop")
        self.AndroidSrc=SrcCode("Android")
        self.WebSrc=SrcCode("Web")
        self.MainWindow=parent
        self.IsDone=False
        self.useAndroid=False
        self.useLinux=False
        self.useWindows=False
        self.useWeb=False
        


    def trace(self,*args):
        self.MainWindow.trace(*args)
        
    def setProgress(self,value,code):
        print("Compile ",code," ",value,"%")
    
    def load(self,fileName):
        pass

    def _selected_android_arms(self, arm7=True, arm8=True):
        arms = []
        if arm7:
            arms.append(0)
        if arm8:
            arms.append(1)
        if len(arms) == 0:
            arms.append(0)
        return arms

    def compileAndroid(self,arm7=True,arm8=True):
        self.MainWindow.ClearConsole()
        self.MainWindow.trace("Compile Android")
        self.MainWindow.trace("Compile:",self.Name)
        self.MainWindow.trace("Path:",self.Dir)
        self.MainWindow.trace("About:",self.About)
        self.MainWindow.trace("Version:",self.Version)


        ARGCC =[]
        ARGCPP =[]
        ARGLD =[]

        ARGCC.append("-I"+self.Dir+os.path.sep+"src")
        ARGCPP.append("-I"+self.Dir+os.path.sep+"src")
        ARGCPP.append("-I"+self.Dir+os.path.sep+"include")
        ARGCC.append("-I"+self.Dir+os.path.sep+"include")
        ARGCPP.append("-I"+self.Dir+os.path.sep+"include"+os.path.sep+"android")
        ARGCC.append("-I"+self.Dir+os.path.sep+"include")
        ARGCC.append("-I"+self.Dir+os.path.sep+"include"+os.path.sep+"android")
        
        ARGCC.append("-I"+"/media/djoker/code/linux/python/projects/CppEditor/modules/python/include/android")
        ARGCPP.append("-I"+"/media/djoker/code/linux/python/projects/CppEditor/modules/python/include/android")
        
        for inc in self.MainSrc.Include:
            self.MainSrc.ARGS.ARGCPP.append("-I"+self.Dir+os.path.sep+inc)
            self.MainSrc.ARGS.ARGCC.append("-I"+self.Dir+os.path.sep+inc)

        for depend in self.Depends:
            mdp = self.MainWindow.getModule(depend)
            if mdp:
                include = mdp.Dir+OSP+"include"
                ARGCC.append("-I"+include)
                ARGCPP.append("-I"+include)

                for inc in mdp.MainSrc.Include:
                    ARGCC.append("-I"+mdp.Dir+OSP+inc)
                    ARGCPP.append("-I"+mdp.Dir+OSP+inc)

                for inc in mdp.AndroidSrc.Include:
                    ARGCC.append("-I"+mdp.Dir+OSP+inc)
                    ARGCPP.append("-I"+mdp.Dir+OSP+inc)

        
        for arg in self.MainSrc.ARGS.ARGCPP:
            ARGCPP.append(arg)

        for arg in self.MainSrc.ARGS.ARGCC:
            ARGCC.append(arg)

        for arg in self.MainSrc.ARGS.ARGLD:                        
            ARGLD.append(arg)

        for arg in self.AndroidSrc.ARGS.ARGCPP:
            ARGCPP.append(arg)

        for arg in self.AndroidSrc.ARGS.ARGCC:
            ARGCC.append(arg)
            
        for arg in self.AndroidSrc.ARGS.ARGLD:                        
            ARGLD.append(arg)




        code = []
        for src in self.MainSrc.Src:
            code.append(self.Dir+os.path.sep+src)
           
        for src in self.AndroidSrc.Src:
            code.append(self.Dir+os.path.sep+src)
        

        self.androidCompileWorkers = []
        for arm in self._selected_android_arms(arm7, arm8):
            self.worker = CompileWorker(self.MainWindow,1,self.Dir,self.Name,code,True,ARGCPP,ARGCC,arm)
            self.worker.traceMessage.connect(self.MainWindow.log)
            self.worker.compilerComplete.connect(self.compilationDone)
            self.worker.start()
            self.androidCompileWorkers.append(self.worker)
        #AndroidCompile(self.MainWindow,self.Dir,self.Name,code ,ARGCC,ARGCPP, ARGLD,2,0,True)
        
        #AndroidCompile(self.MainWindow,self.Dir,self.Name,code ,ARGCC,ARGCPP,0,False)
        #def AndroidCompile(parent,folderRoot ,name, srcs ,CARGS,CPPARGS,arm=0,fullBuild=False):
            
        #AndroidBuild(self.MainWindow,self.Dir,self.Name,ARGLD,2,0)
        
        
        #def AndroidBuild(parent,folderRoot ,name, LDARGS,buildType=0,arm=0):
        
        
                
                
                    
    
    def compileWeb(self):
        self.MainWindow.ClearConsole()
        self.MainWindow.trace("Compile Emscripten")
        self.MainWindow.trace("Compile:",self.Name)
        self.MainWindow.trace("Path:",self.Dir)
        self.MainWindow.trace("About:",self.About)
        self.MainWindow.trace("Version:",self.Version)


        ARGCC =[]
        ARGCPP =[]
        ARGLD =[]

        ARGCC.append("-I"+self.Dir+os.path.sep+"src")
        ARGCPP.append("-I"+self.Dir+os.path.sep+"src")
        ARGCPP.append("-I"+self.Dir+os.path.sep+"include")
        ARGCC.append("-I"+self.Dir+os.path.sep+"include")

        for depend in self.Depends:
            mdp = self.MainWindow.getModule(depend)
            if mdp:
                include = mdp.Dir+OSP+"include"
                ARGCC.append("-I"+include)
                ARGCPP.append("-I"+include)

                for inc in mdp.MainSrc.Include:
                    ARGCC.append("-I"+mdp.Dir+OSP+inc)
                    ARGCPP.append("-I"+mdp.Dir+OSP+inc)

                for inc in mdp.WebSrc.Include:
                    ARGCC.append("-I"+mdp.Dir+OSP+inc)
                    ARGCPP.append("-I"+mdp.Dir+OSP+inc)
        
        for arg in self.MainSrc.ARGS.ARGCPP:
            ARGCPP.append(arg)

        for arg in self.MainSrc.ARGS.ARGCC:
            ARGCC.append(arg)

        for arg in self.MainSrc.ARGS.ARGLD:                        
            ARGLD.append(arg)

        for arg in self.WebSrc.ARGS.ARGCPP:
            ARGCPP.append(arg)

        for arg in self.WebSrc.ARGS.ARGCC:
            ARGCC.append(arg)
            
        for arg in self.WebSrc.ARGS.ARGLD:                        
            ARGLD.append(arg)




        code = []
        for src in self.MainSrc.Src:
            code.append(self.Dir+os.path.sep+src)
           
        for src in self.WebSrc.Src:
            code.append(self.Dir+os.path.sep+src)
        

        self.worker = CompileWorker(self.MainWindow,2,self.Dir,self.Name,code,True,ARGCPP,ARGCC)
        self.worker.traceMessage.connect(self.MainWindow.log)
        self.worker.compilerComplete.connect(self.compilationDone)
        self.worker.start()
        
        



    def clean(self):
        try:
            self.MainWindow.ClearConsole()
            self.MainWindow.trace("Clean:",self.Name)
            #code.append(self.Dir+os.path.sep+src)
            for root, dirs, files in os.walk(self.Dir+os.path.sep+"obj"):
                for file in files:
                    fullName=os.path.join(root, file)
                    isFile = os.path.isfile(fullName)
                    if isFile:
                        os.remove(fullName)
                   
            if  os.path.exists(self.Dir+os.path.sep+"obj"):
                shutil.rmtree(self.Dir+os.path.sep+"obj")
        except Exception as e:
            self.MainWindow.trace('Error on line :',sys.exc_info()[-1].tb_lineno,str(e))
          
 
    def compilationDone(self,msg, code,objs):
        print("Compilation done ",msg,' ', code,'',objs)
        #self.buildDesktop(objs)
        
    def buildingDone(self,msg, code):
        print("Building done ",msg,' ', code)

    def buildDesktop(self):
        self.MainWindow.trace("Build Desktop")
        print("Build Desktop")
        ARGLD = []
        outFolder=self.Dir+os.path.sep+"obj"+os.path.sep+"Linux"+os.path.sep+self.Name

        cppExtencions=['.cpp','xpp']
        useCPP=True
         
        for arg in self.MainSrc.ARGS.ARGLD:                        
            ARGLD.append(arg)
            
            
        for arg in self.DesktopSrc.ARGS.ARGLD:                        
            ARGLD.append(arg)

        mode=2
        if self.Static==False:
            mode=1
        

        
        self.worker = BuildWorker(self.MainWindow,0,self.Dir,self.Name,useCPP,ARGLD,mode)
        self.worker.traceMessage.connect(self.MainWindow.log)
        self.worker.buildingComplete.connect(self.buildingDone)
        self.worker.start()
        
    def buildAndroid(self,arm7=True,arm8=True):
        self.MainWindow.trace("Build Android")
        print("Build Android")
        ARGLD = []
        useCPP=True

        for arg in self.MainSrc.ARGS.ARGLD:                        
            ARGLD.append(arg)
            
        for arg in self.AndroidSrc.ARGS.ARGLD:                        
            ARGLD.append(arg)

        mode=2
        if self.Static==False:
            mode=1
        self.androidBuildWorkers = []
        for arm in self._selected_android_arms(arm7, arm8):
            self.worker = BuildWorker(self.MainWindow,1,self.Dir,self.Name,useCPP,ARGLD,mode,arm)
            self.worker.traceMessage.connect(self.MainWindow.log)
            self.worker.buildingComplete.connect(self.buildingDone)
            self.worker.start()
            self.androidBuildWorkers.append(self.worker)
    def buildWeb(self):
        self.MainWindow.trace("Build Web")
        print("Build Web")
        ARGLD = []
        useCPP=True

        for arg in self.MainSrc.ARGS.ARGLD:                        
            ARGLD.append(arg)
            
        for arg in self.WebSrc.ARGS.ARGLD:                        
            ARGLD.append(arg)

         
        self.worker = BuildWorker(self.MainWindow,2,self.Dir,self.Name,useCPP,ARGLD,2)
        self.worker.traceMessage.connect(self.MainWindow.log)
        self.worker.buildingComplete.connect(self.buildingDone)
        self.worker.start()    
            
    def compileDesktop(self):
        print("*** compile desktop")
        self.MainWindow.ClearConsole()
        self.MainWindow.trace("Compile Desktop")
        self.MainWindow.trace("Compile:",self.Name)
        self.MainWindow.trace("Path:",self.Dir)
        self.MainWindow.trace("About:",self.About)
        self.MainWindow.trace("Version:",self.Version)

        self.MainSrc.ARGS.ARGCC.append("-I"+self.Dir+os.path.sep+"src")
        self.MainSrc.ARGS.ARGCC.append("-I"+self.Dir+os.path.sep+"src")
        self.MainSrc.ARGS.ARGCC.append("-I"+self.Dir+os.path.sep+"include")
        self.MainSrc.ARGS.ARGCC.append("-I"+self.Dir+os.path.sep+"include")


        ARGCC =[]
        ARGCPP =[]
        

        ARGCC.append("-I"+self.Dir+os.path.sep+"src")
        ARGCPP.append("-I"+self.Dir+os.path.sep+"src")
        ARGCPP.append("-I"+self.Dir+os.path.sep+"include")
        ARGCPP.append("-I"+self.Dir+os.path.sep+"include"+os.path.sep+"linux")
        ARGCC.append("-I"+self.Dir+os.path.sep+"include")
        ARGCC.append("-I"+self.Dir+os.path.sep+"include"+os.path.sep+"linux")
        
        osName = "Linux"
        if sys.platform.startswith('windows'):
            osName = "Windows"
        elif sys.platform.startswith('linux'):
            osName = "Linux"
        
        
        for inc in self.MainSrc.Include:
            self.MainSrc.ARGS.ARGCPP.append("-I"+self.Dir+os.path.sep+inc)
            self.MainSrc.ARGS.ARGCC.append("-I"+self.Dir+os.path.sep+inc)
        
        for depend in self.Depends:
            mdp = self.MainWindow.getModule(depend)
            if mdp:
                include = mdp.Dir+OSP+"include"
                ARGCC.append("-I"+include)
                ARGCPP.append("-I"+include)
                
 
                print(mdp.MainSrc.Include)
                print(mdp.DesktopSrc.Include)
                for inc in mdp.MainSrc.Include:
                    ARGCC.append("-I"+mdp.Dir+OSP+inc)
                    ARGCPP.append("-I"+mdp.Dir+OSP+inc)
                
                for inc in mdp.DesktopSrc.Include:
                    ARGCC.append("-I"+mdp.Dir+OSP+inc)
                    ARGCPP.append("-I"+mdp.Dir+OSP+inc)
                    
                
           
        

        for arg in self.MainSrc.ARGS.ARGCPP:
            ARGCPP.append(arg)

        for arg in self.MainSrc.ARGS.ARGCC:
            ARGCC.append(arg)



        for arg in self.DesktopSrc.ARGS.ARGCPP:
            ARGCPP.append(arg)

        for arg in self.DesktopSrc.ARGS.ARGCC:
            ARGCC.append(arg)
            


        code = []
        for src in self.MainSrc.Src:
            code.append(self.Dir+os.path.sep+src)
           
        for src in self.DesktopSrc.Src:
            code.append(self.Dir+os.path.sep+src)
        
        #self.worker=TestWorker(self.MainWindow)
        self.worker = CompileWorker(self.MainWindow,0,self.Dir,self.Name,code,True,ARGCPP,ARGCC)
        self.worker.traceMessage.connect(self.MainWindow.log)
        self.worker.compilerComplete.connect(self.compilationDone)
        self.worker.start()
        

           
           
        

class Project:
    def __init__(self,parent):
        self.Dir=projectsPath
        self.Root=""
        self.FileName="untitled.c"
        self.Name="project"
        self.Modules=[]
        self.Src=SrcCode("Main")
        self.MainWindow=parent
        self.IsDone=False
            
    def trace(self,*args):
        self.MainWindow.trace(*args)
        
    def setProgress(self,value,code):
        print("Compile ",code," ",value,"%")

    def addInclude(self, d):
        if not d in self.Src.Include:
            self.Src.Include.append(d)
        self.Src.Include = list(dict.fromkeys(self.Src.Include))
            
    def addSrc(self,code):
        if not code in self.Src.Src:
            self.Src.Src.append(code)
        self.Src.Src = list(dict.fromkeys(self.Src.Src))

    def removeCode(self):
        self.Src.Src.clear()
            
           
    def removeSrc(self,code):
        if  code in self.Src.Src:
            self.Src.Src.remove(code)

    def addModule(self,code):
        if not code in self.Modules:
            self.Modules.append(code)
            
    def removeModule(self,code):
        if  code in self.Modules:
            self.Modules.remove(code)

    def cleanAndroid(self):
        pass
    def cleanDesktop(self):
        export =self.Dir+os.path.sep+self.Name
        if os.path.isfile(export):
            os.remove(export)
        shutil.rmtree(self.Dir+os.path.sep+"obj"+os.path.sep+"Linux")


    def cleanWeb(self):
        pass
    
    def clean(self,plataform):
        print("clean ",plataform," ",self.Name," ",self.Dir)
        try:
            if plataform==0:
                self.cleanDesktop()
            
            if plataform==1:
                self.cleanAndroid()

            if plataform==2:
                self.cleanWeb()


        except  Exception as error:
            print(" Error  cleaning project :",error) 


    def _androidOutRoot(self):
        return self.Dir + os.path.sep + "Android" + os.path.sep + self.Name + os.path.sep

    def _androidHasNativeLib(self):
        out_root = self.Dir + os.path.sep + "Android" + os.path.sep
        lib_name = "lib" + self.Name + ".so"
        for abi in ("armeabi-v7a", "arm64-v8a"):
            lib_path = out_root + abi + os.path.sep + lib_name
            if os.path.exists(lib_path):
                return True
        return False

    def _resolveAndroidSetup(self):
        android_pack = (self.Src.ARGSANDROID.PACKAGE or "").strip()
        android_activity = (self.Src.ARGSANDROID.ACTIVITY or "").strip()
        android_label = self.Name

        manifest_path = self._androidOutRoot() + "AndroidManifest.xml"
        if os.path.exists(manifest_path):
            try:
                with open(manifest_path, "r", encoding="utf-8") as data:
                    manifest_data = data.read()

                package_match = re.search(r'package="([^"]+)"', manifest_data)
                if package_match and not android_pack:
                    android_pack = package_match.group(1).strip()

                label_match = re.search(r'android:label="([^"]+)"', manifest_data)
                if label_match:
                    value = label_match.group(1).strip()
                    if value:
                        android_label = value

                activity_match = re.search(r'<activity\s+android:name="([^"]+)"', manifest_data)
                if activity_match and not android_activity:
                    android_activity = activity_match.group(1).strip()
            except Exception as e:
                self.trace("Warn: could not parse AndroidManifest.xml: ", type(e).__name__, " ", e)

        android_pack = sanitize_android_package_name(android_pack)
        if not android_activity:
            android_activity = "android.app.NativeActivity"

        if android_activity.startswith("."):
            android_activity = android_pack + android_activity
        elif "." not in android_activity:
            android_activity = android_pack + "." + android_activity

        if not android_label:
            android_label = self.Name

        self.Src.ARGSANDROID.PACKAGE = android_pack
        self.Src.ARGSANDROID.ACTIVITY = android_activity
        return android_pack, android_label, android_activity

    def runAndroid(self):
        android_pack, android_label, android_activity = self._resolveAndroidSetup()
        start_args = ["shell", "am", "start", "-n", android_pack + "/" + android_activity]

        if self._androidHasNativeLib():
            self.trace("Try run app ...")
            code, out, err = runProcess(ANDROID_SDK + "/platform-tools/adb", start_args)
            if code == 0:
                self.trace(out.decode("utf-8"))
                return
            self.trace("Run existing app failed, rebuilding apk: " + err.decode("utf-8"))

        self.trace("Android native libs not found or app not installed, compiling project")
        if not self.compileAndroid():
            self.trace("Android compile failed")
            return

        self.trace("Try run app ...")
        code, out, err = runProcess(ANDROID_SDK + "/platform-tools/adb", start_args)
        if code != 0:
            self.trace("Error  running  apk :" + err.decode("utf-8"))
            return
        self.trace(out.decode("utf-8"))

    

    def runDesktop(self):
        export =self.Dir+os.path.sep+self.Name
        if os.path.isfile(export):
            self.trace("Running  "+export)
            if not self.MainWindow.runner:          
                self.MainWindow.runner = ExecuteWorker(self.MainWindow, export, 0)
                self.MainWindow.runner.traceMessage.connect(self.MainWindow.log)
                self.MainWindow.runner.start()
            else:
                print("is running ??")
                    
            #code, out, err=runProcess(export,[],False)
            #if code!=0:
            #    self.trace(err.decode("utf-8") )
            #    return
            #self.trace(out.decode("utf-8"))
    
    def runWeb(self):
        export = resolveWebExport(self.Dir, self.Name)
        if not export:
            self.trace("Web output not found for ", self.Name, " (trying build)")
            self.compileWeb()
            export = resolveWebExport(self.Dir, self.Name)
            if not export:
                self.trace("Web output not found for ", self.Name)
                self.trace("Expected: ", self.Dir + os.path.sep + "Web" + os.path.sep + self.Name + ".html")
                self.trace("or: ", self.Dir + os.path.sep + "Web" + os.path.sep + self.Name + os.path.sep + self.Name + ".html")
                return
        try:
            self.MainWindow.runner = ExecuteWorker(self.MainWindow, export, 2)
            self.MainWindow.runner.traceMessage.connect(self.MainWindow.log)
            self.MainWindow.runner.start()
        except Exception as e:
            self.trace("Error execute ", type(e).__name__, " ", e)
        

    def run(self,plataform):
        print("run ",plataform," ",self.Name," ",self.Dir)
        try:
            if plataform==0:
                self.runDesktop()
            
            if plataform==1:
                self.runAndroid()

            if plataform==2:
                self.runWeb()
        except  Exception as error:
            print(" Error  run  File:",error)

    def compile(self,plataform):
        print("compile ",plataform," ",self.Name," ",self.Dir)
        print(self.MainWindow.modulesList)
        try:
            if plataform==0:
                return self.compileDesktop()
            
            if plataform==1:
                return self.compileAndroid()

            if plataform==2:
                return self.compileWeb()

        except  Exception as error:
            print(" Error  Compilimg project :",error)
        return False

    def _activeModules(self):
        if len(self.Modules) > 0:
            return self.Modules
        return self.MainWindow.GLOBALMODULES

    def _collectDesktopModuleArgs(self, ARGCC, ARGCPP, ARGLD):
        osName = "Linux"
        if sys.platform.startswith('windows'):
            osName = "Windows"

        useIncludes = []
        useLibsPath = []

        for name in self._activeModules():
            module = self.MainWindow.getModule(name)
            if not module:
                continue

            for depend in module.Depends:
                mdp = self.MainWindow.getModule(depend)
                if not mdp:
                    continue

                include = mdp.Dir + OSP + "include"
                useIncludes.append("-I" + include)
                lib = mdp.Name
                libPath = mdp.Dir + OSP + osName

                for inc in mdp.MainSrc.Include:
                    useIncludes.append("-I" + mdp.Dir + OSP + inc)
                for inc in mdp.DesktopSrc.Include:
                    useIncludes.append("-I" + mdp.Dir + OSP + inc)

                useLibsPath.append("-L" + libPath)

            include = module.Dir + OSP + "include"
            useIncludes.append("-I" + include)
            include = module.Dir + OSP + "include" + OSP + "linux"
            useIncludes.append("-I" + include)

            libPath = module.Dir + OSP + osName
            useLibsPath.append("-L" + libPath)

            for inc in module.MainSrc.Include:
                useIncludes.append("-I" + module.Dir + OSP + inc)
            for inc in module.DesktopSrc.Include:
                useIncludes.append("-I" + module.Dir + OSP + inc)

            lib = module.Name
            pathLib = libPath + OSP + 'lib' + lib + '.a'
            if module.Static == False:
                pathLib = libPath + OSP + 'lib' + lib + '.so'

            for inc in useIncludes:
                ARGCC.append(inc)
                ARGCPP.append(inc)
            for libpath in useLibsPath:
                ARGLD.append(libpath)

            if os.path.exists(pathLib):
                ARGLD.append("-l" + lib)

            for arg in module.MainSrc.ARGS.ARGLD:
                ARGLD.append(arg)
            for arg in module.DesktopSrc.ARGS.ARGLD:
                ARGLD.append(arg)

    def _collectWebModuleArgs(self, ARGCC, ARGCPP, ARGLD):
        osName = "Web"
        useIncludes = []
        useLibsPath = []

        for name in self._activeModules():
            module = self.MainWindow.getModule(name)
            if not module:
                continue

            for depend in module.Depends:
                mdp = self.MainWindow.getModule(depend)
                if not mdp:
                    continue

                include = mdp.Dir + OSP + "include"
                useIncludes.append("-I" + include)
                libPath = mdp.Dir + OSP + osName

                for inc in mdp.MainSrc.Include:
                    useIncludes.append("-I" + mdp.Dir + OSP + inc)
                for inc in mdp.WebSrc.Include:
                    useIncludes.append("-I" + mdp.Dir + OSP + inc)

                useLibsPath.append("-L" + libPath)

            include = module.Dir + OSP + "include"
            useIncludes.append("-I" + include)
            include = module.Dir + OSP + "include" + OSP + "web"
            useIncludes.append("-I" + include)

            libPath = module.Dir + OSP + osName
            useLibsPath.append("-L" + libPath)

            for inc in module.MainSrc.Include:
                useIncludes.append("-I" + module.Dir + OSP + inc)
            for inc in module.WebSrc.Include:
                useIncludes.append("-I" + module.Dir + OSP + inc)

            lib = module.Name
            pathLib = libPath + OSP + 'lib' + lib + '.a'
            if module.Static == False:
                pathLib = libPath + OSP + 'lib' + lib + '.so'

            if module.Static and os.path.exists(pathLib):
                try:
                    if os.path.getsize(pathLib) <= 8:
                        self.trace("Web lib is empty, rebuilding: ", pathLib)
                        self._rebuildWebStaticModuleArchive(module, pathLib)
                except Exception as e:
                    self.trace("Error checking web lib ", pathLib, " ", type(e).__name__, " ", e)

            for inc in useIncludes:
                ARGCC.append(inc)
                ARGCPP.append(inc)
            for libpath in useLibsPath:
                ARGLD.append(libpath)

            if os.path.exists(pathLib):
                ARGLD.append("-l" + lib)

            for arg in module.MainSrc.ARGS.ARGLD:
                ARGLD.append(arg)
            for arg in module.WebSrc.ARGS.ARGLD:
                ARGLD.append(arg)

    def _collectAndroidModuleArgs(self, ARGCC, ARGCPP, ARGLD, arm=0):
        osName = "Android"
        build_output_arm = "armeabi-v7a"
        if arm == 1:
            build_output_arm = "arm64-v8a"

        useIncludes = []
        useLibsPath = []

        for name in self._activeModules():
            module = self.MainWindow.getModule(name)
            if not module:
                module_dir = modulesPath + OSP + name
                include = module_dir + OSP + "include"
                if os.path.exists(include):
                    ARGCC.append("-I" + include)
                    ARGCPP.append("-I" + include)
                include_android = include + OSP + "android"
                if os.path.exists(include_android):
                    ARGCC.append("-I" + include_android)
                    ARGCPP.append("-I" + include_android)

                libPath = module_dir + OSP + osName + OSP + build_output_arm
                pathLib = libPath + OSP + "lib" + name + ".a"
                if os.path.exists(pathLib):
                    ARGLD.append("-L" + libPath)
                    ARGLD.append("-l" + name)
                continue

            for depend in module.Depends:
                mdp = self.MainWindow.getModule(depend)
                if not mdp:
                    continue

                include = mdp.Dir + OSP + "include"
                useIncludes.append("-I" + include)
                libPath = mdp.Dir + OSP + osName + OSP + build_output_arm

                for inc in mdp.MainSrc.Include:
                    useIncludes.append("-I" + mdp.Dir + OSP + inc)
                for inc in mdp.AndroidSrc.Include:
                    useIncludes.append("-I" + mdp.Dir + OSP + inc)

                useLibsPath.append("-L" + libPath)

            include = module.Dir + OSP + "include"
            useIncludes.append("-I" + include)
            include = module.Dir + OSP + "include" + OSP + "android"
            useIncludes.append("-I" + include)

            libPath = module.Dir + OSP + osName + OSP + build_output_arm
            useLibsPath.append("-L" + libPath)

            for inc in module.MainSrc.Include:
                useIncludes.append("-I" + module.Dir + OSP + inc)
            for inc in module.AndroidSrc.Include:
                useIncludes.append("-I" + module.Dir + OSP + inc)

            lib = module.Name
            pathLib = libPath + OSP + 'lib' + lib + '.a'
            if module.Static == False:
                pathLib = libPath + OSP + 'lib' + lib + '.so'

            for inc in useIncludes:
                ARGCC.append(inc)
                ARGCPP.append(inc)
            for libpath in useLibsPath:
                ARGLD.append(libpath)

            if os.path.exists(pathLib):
                ARGLD.append("-l" + lib)

            for arg in module.MainSrc.ARGS.ARGLD:
                ARGLD.append(arg)
            for arg in module.AndroidSrc.ARGS.ARGLD:
                ARGLD.append(arg)

    def _rebuildWebStaticModuleArchive(self, module, archive_path):
        obj_root = module.Dir + OSP + "obj" + OSP + "Web" + OSP + module.Name
        obj_list = []
        for root, dirs, files in os.walk(obj_root, topdown=False):
            for name in files:
                if os.path.splitext(name)[1] == ".o":
                    full_name = root + OSP + name
                    if not full_name in obj_list:
                        obj_list.append(full_name)

        if len(obj_list) <= 0:
            self.trace("No objects to rebuild web lib for module ", module.Name, " at ", obj_root)
            return False

        createFolderTree(os.path.dirname(archive_path))
        if os.path.exists(archive_path):
            try:
                os.remove(archive_path)
            except Exception:
                pass

        args = ["rcs", archive_path]
        for obj in obj_list:
            args.append(obj)

        code, out, err = runProcess(EMAR, args)
        if code != 0:
            self.trace("Error rebuild web lib ", module.Name, ": ", err.decode("utf-8"))
            return False

        if not os.path.exists(archive_path) or os.path.getsize(archive_path) <= 8:
            self.trace("Rebuilt web lib is still empty: ", archive_path)
            return False

        self.trace("Rebuilt Web lib: ", archive_path)
        return True

    def _appendWebTemplateAndAssets(self, ARGLD):
        template = None
        project_shell = (self.Src.ARGSWEB.SHELL or "").strip()
        if project_shell:
            if os.path.isabs(project_shell):
                shell_path = project_shell
            else:
                shell_path = self.Dir + OSP + project_shell

            if os.path.exists(shell_path):
                template = shell_path
            else:
                self.trace("Web shell not found: ", shell_path)

        for name in self._activeModules():
            if template:
                break
            module = self.MainWindow.getModule(name)
            if not module:
                continue
            shell = module.WebSrc.SHELL
            if shell:
                shell_path = module.Dir + OSP + shell
                if os.path.exists(shell_path):
                    template = shell_path
                    break

        if template:
            ARGLD.append("--shell-file")
            ARGLD.append(template)

        preload_entries = [
            ("scripts", "scripts"),
            ("assets", "assets"),
            ("resources", "resources"),
            ("data", "data"),
            ("media", "media"),
        ]
        for folder_name, mount_name in preload_entries:
            host_path = self.Dir + OSP + folder_name
            if os.path.exists(host_path):
                ARGLD.append("--preload-file")
                ARGLD.append(host_path + "@/{}".format(mount_name))

    def compileDesktop(self):
        srcs = []
        buildWithCpp = False
        for src in self.Src.Src:
            if os.path.isfile(src):
                srcs.append(src)
            _, ext = os.path.splitext(src)
            if len(ext) >= 3:
                buildWithCpp = True

        ARGCPP = []
        for arg in self.Src.ARGSDESKTOP.ARGCPP:
            value = arg.strip()
            if len(value) >= 2:
                ARGCPP.append(arg)

        ARGCC = []
        for arg in self.Src.ARGSDESKTOP.ARGCC:
            value = arg.strip()
            if len(value) >= 2:
                ARGCC.append(arg)

        ARGLD = []
        for arg in self.Src.ARGSDESKTOP.ARGLD:
            value = arg.strip()
            if len(value) >= 2:
                ARGLD.append(arg)

        self._collectDesktopModuleArgs(ARGCC, ARGCPP, ARGLD)

        if DesktopCompile(self, self.Dir, self.Name, srcs, ARGCC, ARGCPP, False):
            return DesktopBuild(self, self.Dir, self.Name, buildWithCpp, ARGLD, 0)
        self.trace("Fail compile")
        return False


    def compileAndroid(self):
        print("compile android")
        arm7=True
        arm8=False
        
        buildWithCpp=False
        
        srcs=[]
        for src in self.Src.Src:
            srcs.append(src)
            filename, file_extension = os.path.splitext(src)
            print(file_extension,len(file_extension))
            if len(file_extension)>=3:
                buildWithCpp=True
            
            
        ARGCPP=[]
        for arg in self.Src.ARGSANDROID.ARGCPP: 
            value =arg.strip()
            if len(value)>=2:
                ARGCPP.append(arg)
        ARGCC=[]
        for arg in self.Src.ARGSANDROID.ARGCC: 
            value =arg.strip()
            if len(value)>=2:
                ARGCC.append(arg)
                        
        ARGLD=[]
        for arg in self.Src.ARGSANDROID.ARGLD: 
            value =arg.strip()
            if len(value)>=2:
                ARGLD.append(arg)

        for inc in self.Src.Include:
            value = inc.strip()
            if len(value) >= 2:
                ARGCC.append("-I" + value)
                ARGCPP.append("-I" + value)

        ok = True
        build_any = False

        if arm7:
            build_any = True
            arm7LD = list(ARGLD)
            arm7CC = list(ARGCC)
            arm7CPP = list(ARGCPP)
            self._collectAndroidModuleArgs(arm7CC, arm7CPP, arm7LD, 0)
            if AndroidCompile(self,self.Dir,self.Name,srcs,arm7CC,arm7CPP,0,False):
                ok = AndroidBuild(self,self.Dir,self.Name,buildWithCpp,arm7LD,0,0) and ok
            else:
                ok = False
                
        if arm8:
            build_any = True
            arm8LD = list(ARGLD)
            arm8CC = list(ARGCC)
            arm8CPP = list(ARGCPP)
            self._collectAndroidModuleArgs(arm8CC, arm8CPP, arm8LD, 1)
            if AndroidCompile(self,self.Dir,self.Name,srcs,arm8CC,arm8CPP,1,False):
                ok = AndroidBuild(self,self.Dir,self.Name,buildWithCpp,arm8LD,0,1) and ok
            else:
                ok = False
      
        if not (ok and build_any):
            return False

        android_pack, android_label, android_activity = self._resolveAndroidSetup()
        android_pack, android_label, android_activity = ensure_android_native_template(
            self,
            self.Dir,
            self.Name,
            android_pack,
            android_label,
            android_activity,
        )
        if not androidCompileJavaNative(
            self,
            self.Dir,
            0,
            self.Name,
            android_pack,
            android_label,
            android_activity,
            False,
        ):
            return False

        return True
        
    def compileWeb(self):
        srcs = []
        useCPP = False
        for src in self.Src.Src:
            if os.path.isfile(src):
                srcs.append(src)
            _, ext = os.path.splitext(src)
            if len(ext) >= 3:
                useCPP = True

        ARGCPP = []
        for arg in self.Src.ARGSWEB.ARGCPP:
            value = arg.strip()
            if len(value) >= 2:
                ARGCPP.append(arg)

        ARGCC = []
        for arg in self.Src.ARGSWEB.ARGCC:
            value = arg.strip()
            if len(value) >= 2:
                ARGCC.append(arg)

        ARGLD = []
        for arg in self.Src.ARGSWEB.ARGLD:
            value = arg.strip()
            if len(value) >= 2:
                ARGLD.append(arg)

        for inc in self.Src.Include:
            value = inc.strip()
            if len(value) >= 2:
                ARGCC.append("-I" + value)
                ARGCPP.append("-I" + value)

        self._collectWebModuleArgs(ARGCC, ARGCPP, ARGLD)
        self._appendWebTemplateAndAssets(ARGLD)

        if WebCompile(self, self.Dir, self.Name, srcs, ARGCC, ARGCPP, False):
            if not WebBuild(self, self.Dir, self.Name, useCPP, ARGLD, False):
                self.trace("Web build/link failed for ", self.Name)
                return False
            return True
        self.trace("Web compile failed for ", self.Name)
        return False
            
    




    def load(self,fileName):
        try:
            self.FileName=fileName
            
            self.Name=os.path.splitext(os.path.basename(self.FileName))[0]
            self.Dir=os.path.dirname(os.path.abspath(self.FileName))
            
            
            

            with open(self.FileName, 'r') as data:
                jdata = json.load(data)
                #print(jdata)        
                #self.Dir=jdata["path"] ?????
                self.Modules.clear()
                self.Src.Src.clear()
                self.Src.Include.clear()
                self.Src.ARGS.clear()
                self.Src.ARGSDESKTOP.clear()
                self.Src.ARGSANDROID.clear()
                self.Src.ARGSWEB.clear()

                modules=jdata["Modules"]
                
                for module in modules:
                    self.addModule(module)
                
                self.Name=jdata["Name"]
                self.Dir=jdata["Path"]
                
                self.Root=self.Dir+OSP+os.path.basename(self.FileName)
                
                srcs=jdata["Src"]
                for src in srcs:
                    self.addSrc(src)
                
                include=jdata["Include"]
                for inc in include:
                    self.addInclude(inc)
                
                mainOpts=jdata["Main"]
                desktopOpts=jdata["Desktop"]
                androidOpts=jdata["Android"]
                webOpts    =jdata["Web"]

                self.Src.ARGSANDROID.PACKAGE = str(androidOpts.get("PACKAGE", "") or "")
                self.Src.ARGSANDROID.ACTIVITY = str(androidOpts.get("ACTIVITY", "") or "")
                self.Src.ARGSWEB.SHELL = str(webOpts.get("SHELL", "") or "")

                for arg in mainOpts.get("CPP", []):
                    self.Src.ARGS.addCPP(str(arg))
                for arg in mainOpts.get("CC", []):
                    self.Src.ARGS.addCC(str(arg))
                for arg in mainOpts.get("LD", []):
                    self.Src.ARGS.addLD(str(arg))

                for arg in desktopOpts.get("CPP", []):
                    self.Src.ARGSDESKTOP.addCPP(str(arg))
                for arg in desktopOpts.get("CC", []):
                    self.Src.ARGSDESKTOP.addCC(str(arg))
                for arg in desktopOpts.get("LD", []):
                    self.Src.ARGSDESKTOP.addLD(str(arg))

                for arg in androidOpts.get("CPP", []):
                    self.Src.ARGSANDROID.addCPP(str(arg))
                for arg in androidOpts.get("CC", []):
                    self.Src.ARGSANDROID.addCC(str(arg))
                for arg in androidOpts.get("LD", []):
                    self.Src.ARGSANDROID.addLD(str(arg))

                for arg in webOpts.get("CPP", []):
                    self.Src.ARGSWEB.addCPP(str(arg))
                for arg in webOpts.get("CC", []):
                    self.Src.ARGSWEB.addCC(str(arg))
                for arg in webOpts.get("LD", []):
                    self.Src.ARGSWEB.addLD(str(arg))
                

            self.trace("Load  (",self.Name,") Project")
            self.trace("Path  ",self.Dir)
            self.trace("Root  ",self.Root)


        except Exception as e:
            print('Error load project at line {}'.format(sys.exc_info()[-1].tb_lineno), type(e).__name__, e)  
        

  
    def save(self,fileName):
        if fileName==None:
            fileName=self.FileName
        else:
            self.FileName=fileName

        try:
            if self.Name==None:
                self.Name=os.path.splitext(os.path.basename(self.FileName))[0]
            self.Dir=os.path.dirname(os.path.abspath(self.FileName))
            
            with open(self.FileName, 'w') as f:
                f.write('{ \n')
                
                f.write('"Path" :"'+self.Dir+'",\n')
                
                

                numModules  =len(self.Modules)
                numSrcFiles =len(self.Src.Src)

                



                f.write('"Name":"'+self.Name+'",\n')
                f.write('"Modules":'+str(self.Modules).replace("'",'"')+',\n')
                f.write('"Src":'+str(self.Src.Src).replace("'",'"')+',\n')
                f.write('"Include":'+str(self.Src.Include).replace("'",'"')+',\n')
            

                
       

                f.write('"Main" :{\n')
                f.write('"CPP":'+str(self.Src.ARGS.ARGCPP).replace("'",'"')+',\n')
                f.write('"CC":'+str(self.Src.ARGS.ARGCC).replace("'",'"')+',\n')
                f.write('"LD":'+str(self.Src.ARGS.ARGLD).replace("'",'"')+'\n')
                f.write('},\n')          

                f.write('"Desktop" :{\n')
                f.write('"CPP":'+str(self.Src.ARGSDESKTOP.ARGCPP).replace("'",'"')+',\n')
                f.write('"CC":'+str(self.Src.ARGSDESKTOP.ARGCC).replace("'",'"')+',\n')
                f.write('"LD":'+str(self.Src.ARGSDESKTOP.ARGLD).replace("'",'"')+'\n')
                f.write('},\n')          
                
                f.write('"Android" :{\n')
                f.write('"PACKAGE":"'+self.Src.ARGSANDROID.PACKAGE+'",\n')
                f.write('"ACTIVITY":"'+self.Src.ARGSANDROID.ACTIVITY+'",\n')
                f.write('"CPP":'+str(self.Src.ARGSANDROID.ARGCPP).replace("'",'"')+',\n')
                f.write('"CC":'+str(self.Src.ARGSANDROID.ARGCC).replace("'",'"')+',\n')
                f.write('"LD":'+str(self.Src.ARGSANDROID.ARGLD).replace("'",'"')+'\n')
                f.write('},\n')          
                    
                f.write('"Web" :{\n')
                f.write('"SHELL":"'+self.Src.ARGSWEB.SHELL+'",\n')
                f.write('"CPP":'+str(self.Src.ARGSWEB.ARGCPP).replace("'",'"')+',\n')
                f.write('"CC":'+str(self.Src.ARGSWEB.ARGCC).replace("'",'"')+',\n')
                f.write('"LD":'+str(self.Src.ARGSWEB.ARGLD).replace("'",'"')+'\n')
                f.write('}\n')          
                

                f.write('}\n')

        except Exception as e:
            print('Error save project at line {}'.format(sys.exc_info()[-1].tb_lineno), type(e).__name__, e)  

    def LinuxBuild(self, folderRoot ,appName, useCPP):
            if  not folderRoot or not appName:
                return False
            
            outFolder=folderRoot+os.path.sep+"Linux"+os.path.sep+appName        
            #print("Build linux", root,"   ",appName)
            
            ListOBJS=[]
            
            for root, directories, files in os.walk(outFolder, topdown=True):
                for name in files:
                    ext = os.path.splitext(name)[1]
                    if (ext==".o"):
                        if not name in ListOBJS:
                            ListOBJS.append(root+OSP+name)


            if len(ListOBJS)<=0:
                return False
                
            args=[]
            cType = "gcc"
            if useCPP:
                    cType="g++"
            self.MainWindow.trace("Build Linux (",appName,") aplication")
            args.append("-o")
            export =folderRoot+os.path.sep+appName
            args.append(export)
            objs=""
            for obj in ListOBJS:
                objs+=obj+' '
                args.append(obj)

            for arg in self.Src.ARGSDESKTOP.ARGLD:
                value =arg.strip()
                if len(value)>1:
                    args.append(value)
            rootFolder=os.getcwd()+os.path.sep+"libs"+os.path.sep+"linux"
            args.append("-L"+rootFolder)
            
            
            
            final_cmd =str(args)
            
            final_cmd=final_cmd.replace("'",' ')
            final_cmd=final_cmd.replace(",",' ')
            final_cmd=final_cmd.replace("[",' ')
            final_cmd=final_cmd.replace("]",' ')
            
            code, out, err=runProcess(cType,args)
            self.trace(cType," ",final_cmd)
            
            code, out, err=runProcess(cType,args)
            if code!=0:
                self.trace(err.decode("utf-8") )
                self.trace("Operation Fail  .. ")
                return False
            self.trace("Done :) ")
            return True
                                

    def DesktopCompile(self,folderRoot ,name):
        useCPP=False
        lineY=0
        lineX=0
        print("Linux compile to ",folderRoot," app ",name," ", self.Src.Src)
        #if not folderRoot or not name:
        #    return False
        
        outFolder=folderRoot+os.path.sep+"obj"+os.path.sep+"Linux"+os.path.sep+name
        createFolderTree(outFolder)


        objsList=[]
        
        srcs=[]

        for src in self.Src.Src:
            
            srcs.append(src)

        for src in srcs:
            if not os.path.isfile(src):
                self.trace("File ",src," not exists ..")
                continue
        
            args=[]
            srcFolder = os.path.dirname(os.path.abspath(src))
            objFolder =outFolder +  srcFolder.replace(folderRoot,"")
            
            createFolderTree(objFolder)
            filename, file_extension = os.path.splitext(src)
            basename = os.path.basename(src)
            basename_without_ext = os.path.splitext(os.path.basename(src))[0]
            objName = objFolder+os.path.sep+basename_without_ext+".o"
            objsList.append(objName)
            src_modified_time = os.path.getmtime(src)
            src_convert_time = time.ctime(src_modified_time)

            cType = "gcc"
            if len(file_extension)>=3:
                    cType="g++"
                    useCPP=True
                    
            
 
            if os.path.exists(objName):
                obj_modified_time = os.path.getmtime(objName)
                obj_convert_time   = time.ctime(obj_modified_time)
                if (src_convert_time<obj_convert_time):
                    self.trace("Skip  file"+ src)
                    continue
                
                    
        
            
            #self.MainWindow.trace ("Compile : ",cType," ",os.path.basename(src)," > ",os.path.basename(objName))

                
                    
            args.append("-c")
            args.append(src)
            args.append("-o")
            args.append(objName)
            

            if useCPP:
                for arg in self.Src.ARGSDESKTOP.ARGCPP: 
                    value =arg.strip()
                    if len(value)>=2:
                        args.append(arg)
                        
            else:
                for arg in self.Src.ARGSDESKTOP.ARGCC: 
                    value =arg.strip()
                    if len(value)>=2:
                        args.append(arg)
                        
            
            args.append("-fPIC")

                            
            final_cmd =str(args)
            
            final_cmd=final_cmd.replace("'",' ')
            final_cmd=final_cmd.replace(",",' ')
            final_cmd=final_cmd.replace("[",' ')
            final_cmd=final_cmd.replace("]",' ')
            
            code, out, err=runProcess(cType,args)
            self.trace(cType," ",final_cmd)
            
            
            #print("err: '{}'".format(str(err)))
            print(code)
            if code!=0:
                self.trace(err.decode("utf-8") )
                rexp=':(.*?):(.*?): error: '
                #match = re.compile(rexp, re.DOTALL | re.IGNORECASE).findall(err.decode("utf-8"))
                erro =re.search(rexp,err.decode("utf-8"))
                
                try:    
                    if erro:
                        linhas = erro.group().split(":")
                        lineX=int(linhas[2])
                        lineY=int(linhas[1])
                        self.MainWindow.setCursor(lineY-1,lineX-1)        
                        #self.trace("Compile Error line",str(lineY)," position:",str(lineX))

                except  Exception as error:
                    self.trace("Error ",error)
                
                return None
            self.trace(out.decode("utf-8") )
        self.trace("Compiling complete ")
        return objsList
        
    
             
    def WebBuild(self,folderRoot ,appName, useCPP ):
        if  not folderRoot or not appName:
           return False
            
        args=[]
        outFolder=folderRoot+os.path.sep+"Web"+os.path.sep+appName
        createFolderTree(outFolder)
        
        
        root =folderRoot+OSP+"obj"+ OSP+"Web"+OSP
            #print("Build linux", root,"   ",appName)
            
        ListOBJS=[]
        
        for root, directories, files in os.walk(root, topdown=True):
            for name in files:
                ext = os.path.splitext(name)[1]
                if (ext==".o"):
                    if not name in ListOBJS:
                        ListOBJS.append(root+OSP+name)


        if len(ListOBJS)<=0:
            return False
                
        cType = EMCC
        if useCPP:
                cType=EMCPP
                
        self.trace("Build EMSDK (",appName,") aplication")
        args.append("-o")
        
        export = outFolder+os.path.sep+appName+".html"
        args.append(export)
        objs=""
        for obj in ListOBJS:
            objs+=obj+' '
            args.append(obj)
        #args.append('-s')


        for arg in self.Src.ARGSWEB.ARGLD:
            value =arg.strip()
            if len(value)>1:
                args.append(value)
        rootFolder=os.getcwd()+os.path.sep+"libs"+os.path.sep+"web"
        args.append("-L"+rootFolder)
           
            
        final_cmd =str(args)
        final_cmd=final_cmd.replace("'",' ')
        final_cmd=final_cmd.replace(",",' ')
        final_cmd=final_cmd.replace("[",' ')
        final_cmd=final_cmd.replace("]",' ')
        self.trace(cType," ",final_cmd)
            
        code, out, err=runProcess(cType,args)
        if code!=0:
            self.trace(err.decode("utf-8") )
            self.trace("Operation Fail  .. ")
            return False
        return True
                
                
  
            
            
    def WebCompile(self,folderRoot ,name):
        useCPP=False
        if not folderRoot or not name:
            return False

        outFolder=folderRoot+os.path.sep+"obj"+os.path.sep+"Web"+os.path.sep+name
        createFolderTree(outFolder)
        objsList=[]
        
        srcs=[]
        for src in self.Src.Src:
            srcs.append(src)


        for src in srcs:
            if not os.path.isfile(src):
                self.trace("File ",src,"not exists")
                continue
        
            args=[]
            srcFolder = os.path.dirname(os.path.abspath(src))
            objFolder =outFolder +  srcFolder.replace(folderRoot,"")
            
            createFolderTree(objFolder)
            filename, file_extension = os.path.splitext(src)
            basename = os.path.basename(src)
            basename_without_ext = os.path.splitext(os.path.basename(src))[0]
            objName = objFolder+os.path.sep+basename_without_ext+".o"
            objsList.append(objName)
            src_modified_time = os.path.getmtime(src)
            src_convert_time = time.ctime(src_modified_time)

            cType = EMCC
            if len(file_extension)>=3:
                    cType=EMCPP
                    useCPP=True
                    
            
       
            if os.path.exists(objName):
                obj_modified_time = os.path.getmtime(objName)
                obj_convert_time   = time.ctime(obj_modified_time)
                if (src_convert_time<obj_convert_time):
                    self.trace("Skip  file"+ src)
                    continue
            
                    
        
            
            

                
                    
            args.append("-c")
            args.append(src)
            args.append("-o")
            args.append(objName)
            

            if useCPP:
                for arg in self.Src.ARGSWEB.ARGCPP: 
                    value =arg.strip()
                    if len(value)>=2:
                        args.append(arg)
 
            else:
                for arg in self.Src.ARGSWEB.ARGCC: 
                    value =arg.strip()
                    if len(value)>=2:
                        args.append(arg)
                        


                            
            final_cmd =str(args)
            final_cmd=final_cmd.replace("'",' ')
            final_cmd=final_cmd.replace(",",' ')
            final_cmd=final_cmd.replace("[",' ')
            final_cmd=final_cmd.replace("]",' ')
            self.trace(cType," ",final_cmd)
            code, out, err=runProcess(cType,args)
            #print("err: '{}'".format(str(err)))
            #print("exit: {}".format(code))
            if code!=0:
                self.trace(err.decode("utf-8") )
                rexp=':(.*?):(.*?): error: '
                #match = re.compile(rexp, re.DOTALL | re.IGNORECASE).findall(err.decode("utf-8"))
                erro =re.search(rexp,err.decode("utf-8"))
                #print(erro)
                try:    
                    lineY=0
                    lineX=0
                    if erro:
                        linhas = erro.group().split(":")
                        lineX=int(linhas[2])
                        lineY=int(linhas[1])
                        self.MainWindow.setCursor(lineY-1,lineX-1)       
                        return None
                except  Exception as error:
                    self.trace("Error ",error)
                
            self.trace(out.decode("utf-8") )
        self.trace("Compiling completed")
        return objsList
        


class TabEditor(QWidget):
    def __init__(self):
        super(TabEditor, self).__init__()
            
        

class ProjectOptionsWindow(QDialog):
    def __init__(self,parent,plataform):
        super(ProjectOptionsWindow, self).__init__(parent)
        self.mainWindow=parent
        self.Plataform=plataform
        Dialog=self
        Dialog.resize(820, 527)
        sizePolicy = QSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(Dialog.sizePolicy().hasHeightForWidth())
        Dialog.setSizePolicy(sizePolicy)
        Dialog.setMaximumSize(QSize(820, 527))
        Dialog.setWindowTitle(u"Dialog")
        self.gridLayout_5 = QGridLayout(Dialog)
        self.gridLayout_5.setObjectName(u"gridLayout_5")
        self.groupBoxBuildOptions = QGroupBox(Dialog)
        self.groupBoxBuildOptions.setObjectName(u"groupBoxBuildOptions")
        sizePolicy1 = QSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)
        sizePolicy1.setHorizontalStretch(0)
        sizePolicy1.setVerticalStretch(0)
        sizePolicy1.setHeightForWidth(self.groupBoxBuildOptions.sizePolicy().hasHeightForWidth())
        self.groupBoxBuildOptions.setSizePolicy(sizePolicy1)
        self.groupBoxBuildOptions.setTitle(u"Compile & Build ARGS Options")
        self.gridLayout_2 = QGridLayout(self.groupBoxBuildOptions)
        self.gridLayout_2.setObjectName(u"gridLayout_2")
        self.label = QLabel(self.groupBoxBuildOptions)
        self.label.setObjectName(u"label")
        self.label.setText(u"CPP")

        self.gridLayout_2.addWidget(self.label, 0, 0, 1, 1)

        self.lineEditLDArg = QLineEdit(self.groupBoxBuildOptions)
        self.lineEditLDArg.setObjectName(u"lineEditLDArg")
        self.lineEditLDArg.setText(u"")

        self.gridLayout_2.addWidget(self.lineEditLDArg, 2, 1, 1, 1)

        self.label_2 = QLabel(self.groupBoxBuildOptions)
        self.label_2.setObjectName(u"label_2")
        self.label_2.setText(u"CC")

        self.gridLayout_2.addWidget(self.label_2, 1, 0, 1, 1)

        self.lineEditCCArg = QLineEdit(self.groupBoxBuildOptions)
        self.lineEditCCArg.setObjectName(u"lineEditCCArg")
        self.lineEditCCArg.setText(u"")

        self.gridLayout_2.addWidget(self.lineEditCCArg, 1, 1, 1, 1)

        self.label_3 = QLabel(self.groupBoxBuildOptions)
        self.label_3.setObjectName(u"label_3")
        self.label_3.setText(u"LINK")

        self.gridLayout_2.addWidget(self.label_3, 2, 0, 1, 1)

        self.lineEditCPPArg = QLineEdit(self.groupBoxBuildOptions)
        self.lineEditCPPArg.setObjectName(u"lineEditCPPArg")
        self.lineEditCPPArg.setText(u"")

        self.gridLayout_2.addWidget(self.lineEditCPPArg, 0, 1, 1, 1)


        self.gridLayout_5.addWidget(self.groupBoxBuildOptions, 0, 0, 1, 4)

        self.groupBox_3 = QGroupBox(Dialog)
        self.groupBox_3.setObjectName(u"groupBox_3")
        sizePolicy1.setHeightForWidth(self.groupBox_3.sizePolicy().hasHeightForWidth())
        self.groupBox_3.setSizePolicy(sizePolicy1)
        self.groupBox_3.setTitle(u"Options")
        self.gridLayout_4 = QGridLayout(self.groupBox_3)
        self.gridLayout_4.setObjectName(u"gridLayout_4")
        self.groupBoxWeb = QGroupBox(self.groupBox_3)
        self.groupBoxWeb.setObjectName(u"groupBoxWeb")
        sizePolicy2 = QSizePolicy(QSizePolicy.Preferred, QSizePolicy.Fixed)
        sizePolicy2.setHorizontalStretch(0)
        sizePolicy2.setVerticalStretch(0)
        sizePolicy2.setHeightForWidth(self.groupBoxWeb.sizePolicy().hasHeightForWidth())
        self.groupBoxWeb.setSizePolicy(sizePolicy2)
        self.groupBoxWeb.setTitle(u"Web")
        self.gridLayout_6 = QGridLayout(self.groupBoxWeb)
        self.gridLayout_6.setObjectName(u"gridLayout_6")
        self.groupBox_7 = QGroupBox(self.groupBoxWeb)
        self.groupBox_7.setObjectName(u"groupBox_7")
        sizePolicy1.setHeightForWidth(self.groupBox_7.sizePolicy().hasHeightForWidth())
        self.groupBox_7.setSizePolicy(sizePolicy1)
        self.groupBox_7.setTitle(u"")
        self.gridLayout = QGridLayout(self.groupBox_7)
        self.gridLayout.setObjectName(u"gridLayout")
        self.checkBox = QCheckBox(self.groupBox_7)
        self.checkBox.setObjectName(u"checkBox")
        self.checkBox.setText(u"Add  Assets Folder")

        self.gridLayout.addWidget(self.checkBox, 0, 0, 1, 2)

        self.label_7 = QLabel(self.groupBox_7)
        self.label_7.setObjectName(u"label_7")
        self.label_7.setText(u"Folder Name")

        self.gridLayout.addWidget(self.label_7, 1, 0, 1, 1)

        self.lineEdit_3 = QLineEdit(self.groupBox_7)
        self.lineEdit_3.setObjectName(u"lineEdit_3")

        self.gridLayout.addWidget(self.lineEdit_3, 1, 1, 1, 1)


        self.gridLayout_6.addWidget(self.groupBox_7, 0, 0, 1, 1)

        self.groupBox_8 = QGroupBox(self.groupBoxWeb)
        self.groupBox_8.setObjectName(u"groupBox_8")
        sizePolicy1.setHeightForWidth(self.groupBox_8.sizePolicy().hasHeightForWidth())
        self.groupBox_8.setSizePolicy(sizePolicy1)
        self.gridLayout_3 = QGridLayout(self.groupBox_8)
        self.gridLayout_3.setObjectName(u"gridLayout_3")
        self.checkBox_2 = QCheckBox(self.groupBox_8)
        self.checkBox_2.setObjectName(u"checkBox_2")
        self.checkBox_2.setText(u"Use HTML Shell")

        self.gridLayout_3.addWidget(self.checkBox_2, 0, 0, 1, 2)

        self.label_8 = QLabel(self.groupBox_8)
        self.label_8.setObjectName(u"label_8")
        self.label_8.setText(u"File Name")

        self.gridLayout_3.addWidget(self.label_8, 1, 0, 1, 1)

        self.lineEdit_4 = QLineEdit(self.groupBox_8)
        self.lineEdit_4.setObjectName(u"lineEdit_4")

        self.gridLayout_3.addWidget(self.lineEdit_4, 1, 1, 1, 1)


        self.gridLayout_6.addWidget(self.groupBox_8, 1, 0, 1, 1)


        self.gridLayout_4.addWidget(self.groupBoxWeb, 0, 0, 1, 1)

        self.groupBoxAndroid = QGroupBox(self.groupBox_3)
        self.groupBoxAndroid.setObjectName(u"groupBoxAndroid")
        sizePolicy2.setHeightForWidth(self.groupBoxAndroid.sizePolicy().hasHeightForWidth())
        self.groupBoxAndroid.setSizePolicy(sizePolicy2)
        self.groupBoxAndroid.setTitle(u"Android")
        self.gridLayout_7 = QGridLayout(self.groupBoxAndroid)
        self.gridLayout_7.setObjectName(u"gridLayout_7")
        self.groupBox_4 = QGroupBox(self.groupBoxAndroid)
        self.groupBox_4.setObjectName(u"groupBox_4")
        sizePolicy1.setHeightForWidth(self.groupBox_4.sizePolicy().hasHeightForWidth())
        self.groupBox_4.setSizePolicy(sizePolicy1)
        self.groupBox_4.setTitle(u"")
        self.horizontalLayout_5 = QHBoxLayout(self.groupBox_4)
        self.horizontalLayout_5.setObjectName(u"horizontalLayout_5")
        self.label_5 = QLabel(self.groupBox_4)
        self.label_5.setObjectName(u"label_5")
        self.label_5.setText(u"Activity")

        self.horizontalLayout_5.addWidget(self.label_5)

        self.lineEdit = QLineEdit(self.groupBox_4)
        self.lineEdit.setObjectName(u"lineEdit")

        self.horizontalLayout_5.addWidget(self.lineEdit)


        self.gridLayout_7.addWidget(self.groupBox_4, 0, 0, 1, 1)

        self.groupBox_6 = QGroupBox(self.groupBoxAndroid)
        self.groupBox_6.setObjectName(u"groupBox_6")
        sizePolicy1.setHeightForWidth(self.groupBox_6.sizePolicy().hasHeightForWidth())
        self.groupBox_6.setSizePolicy(sizePolicy1)
        self.groupBox_6.setTitle(u"")
        self.horizontalLayout_6 = QHBoxLayout(self.groupBox_6)
        self.horizontalLayout_6.setObjectName(u"horizontalLayout_6")
        self.label_6 = QLabel(self.groupBox_6)
        self.label_6.setObjectName(u"label_6")
        self.label_6.setText(u"Package")

        self.horizontalLayout_6.addWidget(self.label_6)

        self.lineEdit_2 = QLineEdit(self.groupBox_6)
        self.lineEdit_2.setObjectName(u"lineEdit_2")

        self.horizontalLayout_6.addWidget(self.lineEdit_2)


        self.gridLayout_7.addWidget(self.groupBox_6, 1, 0, 1, 1)

        self.groupBox_9 = QGroupBox(self.groupBoxAndroid)
        
        self.groupBox_9.setObjectName(u"groupBox_9")
        sizePolicy1.setHeightForWidth(self.groupBox_9.sizePolicy().hasHeightForWidth())
        self.groupBox_9.setSizePolicy(sizePolicy1)
        self.groupBox_9.setTitle(u"")
        self.horizontalLayout_7 = QHBoxLayout(self.groupBox_9)
        self.horizontalLayout_7.setObjectName(u"horizontalLayout_7")
        self.pushButton_2 = QPushButton(self.groupBox_9)
        self.pushButton_2.setObjectName(u"pushButton_2")
        self.pushButton_2.setText(u"Templates")

        self.horizontalLayout_7.addWidget(self.pushButton_2)

        self.pushButton_3 = QPushButton(self.groupBox_9)
        self.pushButton_3.setObjectName(u"pushButton_3")
        self.pushButton_3.setText(u"Resources")

        self.horizontalLayout_7.addWidget(self.pushButton_3)

        self.pushButton_4 = QPushButton(self.groupBox_9)
        self.pushButton_4.setObjectName(u"pushButton_4")
        self.pushButton_4.setText(u"PushButton")

        self.horizontalLayout_7.addWidget(self.pushButton_4)


        self.gridLayout_7.addWidget(self.groupBox_9, 2, 0, 1, 1)


        self.gridLayout_4.addWidget(self.groupBoxAndroid, 0, 1, 1, 1)


        self.gridLayout_5.addWidget(self.groupBox_3, 2, 0, 1, 4)

        self.groupBox = QGroupBox(Dialog)
        self.groupBox.setObjectName(u"groupBox")
        sizePolicy3 = QSizePolicy(QSizePolicy.Minimum, QSizePolicy.Fixed)
        sizePolicy3.setHorizontalStretch(0)
        sizePolicy3.setVerticalStretch(0)
        sizePolicy3.setHeightForWidth(self.groupBox.sizePolicy().hasHeightForWidth())
        self.groupBox.setSizePolicy(sizePolicy3)
        self.groupBox.setTitle(u"")
        self.horizontalLayout_2 = QHBoxLayout(self.groupBox)
        self.horizontalLayout_2.setObjectName(u"horizontalLayout_2")
        self.radioButton = QRadioButton(self.groupBox)
        self.radioButton.setObjectName(u"radioButton")
        self.radioButton.setText(u"Desktop")

        self.horizontalLayout_2.addWidget(self.radioButton)

        self.radioButton_2 = QRadioButton(self.groupBox)
        self.radioButton_2.setObjectName(u"radioButton_2")
        self.radioButton_2.setText(u"Android")

        self.horizontalLayout_2.addWidget(self.radioButton_2)

        self.radioButton_3 = QRadioButton(self.groupBox)
        self.radioButton_3.setObjectName(u"radioButton_3")
        self.radioButton_3.setText(u"Emscripten")

        self.horizontalLayout_2.addWidget(self.radioButton_3)


        self.gridLayout_5.addWidget(self.groupBox, 3, 0, 1, 2)

        self.horizontalSpacer = QSpacerItem(210, 20, QSizePolicy.Expanding, QSizePolicy.Minimum)

        self.gridLayout_5.addItem(self.horizontalSpacer, 3, 2, 1, 1)

        self.groupBox_2 = QGroupBox(Dialog)
        self.groupBox_2.setObjectName(u"groupBox_2")
        sizePolicy3.setHeightForWidth(self.groupBox_2.sizePolicy().hasHeightForWidth())
        self.groupBox_2.setSizePolicy(sizePolicy3)
        self.groupBox_2.setTitle(u"")
        self.horizontalLayout = QHBoxLayout(self.groupBox_2)
        self.horizontalLayout.setObjectName(u"horizontalLayout")
        self.pushButton = QPushButton(self.groupBox_2)
        self.pushButton.setObjectName(u"pushButton")
        self.pushButton.setText(u"Save")

        self.horizontalLayout.addWidget(self.pushButton)

        self.pushButtonCancel = QPushButton(self.groupBox_2)
        self.pushButtonCancel.setObjectName(u"pushButtonCancel")
        self.pushButtonCancel.setText(u"Cancel")

        self.horizontalLayout.addWidget(self.pushButtonCancel)

        self.pushButtonOk = QPushButton(self.groupBox_2)
        self.pushButtonOk.setObjectName(u"pushButtonOk")
        self.pushButtonOk.setText(u"OK")

        self.horizontalLayout.addWidget(self.pushButtonOk)


        self.gridLayout_5.addWidget(self.groupBox_2, 3, 3, 1, 1)

        self.horizontalSpacer_2 = QSpacerItem(79, 20, QSizePolicy.Expanding, QSizePolicy.Minimum)

        self.gridLayout_5.addItem(self.horizontalSpacer_2, 1, 3, 1, 1)

        self.horizontalSpacer_3 = QSpacerItem(40, 20, QSizePolicy.Expanding, QSizePolicy.Minimum)

        self.gridLayout_5.addItem(self.horizontalSpacer_3, 1, 0, 1, 1)

        self.groupBox_5 = QGroupBox(Dialog)
        self.groupBox_5.setObjectName(u"groupBox_5")
        sizePolicy.setHeightForWidth(self.groupBox_5.sizePolicy().hasHeightForWidth())
        self.groupBox_5.setSizePolicy(sizePolicy)
        self.groupBox_5.setTitle(u"")
        self.horizontalLayout_3 = QHBoxLayout(self.groupBox_5)
        self.horizontalLayout_3.setObjectName(u"horizontalLayout_3")
        self.label_4 = QLabel(self.groupBox_5)
        self.label_4.setObjectName(u"label_4")
        self.label_4.setText(u"Name")

        self.horizontalLayout_3.addWidget(self.label_4)

        self.lineEditName = QLineEdit(self.groupBox_5)
        self.lineEditName.setObjectName(u"lineEditName")

        self.horizontalLayout_3.addWidget(self.lineEditName)


        self.gridLayout_5.addWidget(self.groupBox_5, 1, 1, 1, 2)
        
        self.groupBoxBuildOptions.setFlat(True)
        self.groupBox_3.setFlat(True)
        self.groupBox_4.setFlat(True)
        self.groupBox_6.setFlat(True)

        self.pushButtonCancel.pressed.connect(self.onCancel)
        self.pushButton.pressed.connect(self.saveOption)
        self.pushButtonOk.pressed.connect(self.saveAndClose)
        
        self.radioButton.toggled.connect(lambda:self.switchClick(self.radioButton))
        self.radioButton_2.toggled.connect(lambda:self.switchClick(self.radioButton_2))
        self.radioButton_3.toggled.connect(lambda:self.switchClick(self.radioButton_3))
        QMetaObject.connectSlotsByName(Dialog)
        
    def load(self,project):
        self.project=project
        self.switchPlatform()
        self.loadOption()
        
        self.lineEditName.setText(self.project.Name)
        if self.Plataform==0:
            self.radioButton.setChecked(True)
        if self.Plataform==1:
            self.radioButton_2.setChecked(True)
        if self.Plataform==2:
            self.radioButton_3.setChecked(True)
        self.show()
        
        
    def switchClick(self,b):
        if self.radioButton.isChecked():
            self.Plataform=0
        if self.radioButton_2.isChecked():
            self.Plataform=1
        if self.radioButton_3.isChecked():
            self.Plataform=2
      
        self.switchPlatform()
        #self.saveOption()
        self.loadOption()
        
            
    def switchPlatform(self):
        plat = "Dektop"
        if self.Plataform==0:
            self.setWindowTitle(u"Desktop Compiler Options")
            self.groupBoxWeb.setEnabled(False)
            self.groupBoxAndroid.setEnabled(False)
        if self.Plataform==1:
            self.setWindowTitle(u"Android Compiler Options")
            self.groupBoxAndroid.setEnabled(True)
            self.groupBoxWeb.setEnabled(False)
            plat = "Android"
        if self.Plataform==2:
            self.setWindowTitle(u"Emscripten Compiler Options")
            self.groupBoxWeb.setEnabled(True)
            self.groupBoxAndroid.setEnabled(False)
            plat = "Emscripten"
        self.groupBoxBuildOptions.setTitle("Compiler ("+plat+") Options")

        
    
    def loadOption(self):
            args =  None
            if self.Plataform==0:
                args=self.project.Src.ARGSDESKTOP
            if self.Plataform==1:
                args=self.project.Src.ARGSANDROID
            if self.Plataform==2:
                args=self.project.Src.ARGSWEB
 
            self.lineEditLDArg.setText('')
            self.lineEditCCArg.setText('')
            self.lineEditCPPArg.setText('')
            argument=""
            for arg in args.ARGLD:
                argument+=arg+" "
            self.lineEditLDArg.setText(argument)
            argument=""
            for arg in args.ARGCC:
                argument+=arg+" "
            self.lineEditCCArg.setText(argument)
            argument=""
            for arg in args.ARGCPP:
                argument+=arg+" "
            self.lineEditCPPArg.setText(argument)
        
            
        


                    
    def saveOption(self):
        cppArgs = self.lineEditCPPArg.text().strip().split("-")
        ccArgs   = self.lineEditCCArg.text().strip().split("-")
        ldArgs   = self.lineEditLDArg.text().strip().split("-")
        self.project.Name=self.lineEditName.text().strip() #strip ?? yes?? :P
        
        args =  None
        if self.Plataform==0:
            args=self.project.Src.ARGSDESKTOP
            
        if self.Plataform==1:
            args=self.project.Src.ARGSANDROID
        if self.Plataform==2:
            args=self.project.Src.ARGSWEB
        args.clear()
        for arg in cppArgs:
            value ="-"+arg.strip()
            if len(value)>=2:
                args.addCPP(value)
            
        for arg in ccArgs:
            value ="-"+arg.strip()
            if len(value)>=2:
                args.addCC(value)

        for arg in ldArgs:
            value ="-"+arg.strip()
        if len(value)>=2:
                args.addLD(value)
        self.close()
        
        
       
        
    def saveAndClose(self):
        self.saveOption()
        self.project.save(None)
        self.close()
        
    def onCancel(self):
        self.close()

class CompileOptionsWindow(QDialog):
    def __init__(self,parent,plataform):
        super(CompileOptionsWindow, self).__init__(parent)
        self.mainWindow=parent
        self.Plataform=plataform
        Dialog=self
        Dialog.resize(639, 233)
        sizePolicy = QSizePolicy(QSizePolicy.Maximum, QSizePolicy.Minimum)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(Dialog.sizePolicy().hasHeightForWidth())
        Dialog.setSizePolicy(sizePolicy)
        Dialog.setWindowTitle(u"Dialog")
        self.gridLayout_3 = QGridLayout(Dialog)
        self.gridLayout_3.setObjectName(u"gridLayout_3")
        self.groupBoxBuildOptions = QGroupBox(Dialog)
        self.groupBoxBuildOptions.setObjectName(u"groupBoxBuildOptions")
        sizePolicy1 = QSizePolicy(QSizePolicy.Expanding, QSizePolicy.Minimum)
        sizePolicy1.setHorizontalStretch(0)
        sizePolicy1.setVerticalStretch(0)
        sizePolicy1.setHeightForWidth(self.groupBoxBuildOptions.sizePolicy().hasHeightForWidth())
        self.groupBoxBuildOptions.setSizePolicy(sizePolicy1)
        self.groupBoxBuildOptions.setTitle(u"Compile & Build ARGS Options")
        self.gridLayout_2 = QGridLayout(self.groupBoxBuildOptions)
        self.gridLayout_2.setObjectName(u"gridLayout_2")
        self.label = QLabel(self.groupBoxBuildOptions)
        self.label.setObjectName(u"label")
        self.label.setText(u"CPP")

        self.gridLayout_2.addWidget(self.label, 0, 0, 1, 1)

        self.lineEditLDArg = QLineEdit(self.groupBoxBuildOptions)
        self.lineEditLDArg.setObjectName(u"lineEditLDArg")
        self.lineEditLDArg.setText(u"")

        self.gridLayout_2.addWidget(self.lineEditLDArg, 2, 1, 1, 1)

        self.label_2 = QLabel(self.groupBoxBuildOptions)
        self.label_2.setObjectName(u"label_2")
        self.label_2.setText(u"CC")

        self.gridLayout_2.addWidget(self.label_2, 1, 0, 1, 1)

        self.lineEditCCArg = QLineEdit(self.groupBoxBuildOptions)
        self.lineEditCCArg.setObjectName(u"lineEditCCArg")
        self.lineEditCCArg.setText(u"")

        self.gridLayout_2.addWidget(self.lineEditCCArg, 1, 1, 1, 1)

        self.label_3 = QLabel(self.groupBoxBuildOptions)
        self.label_3.setObjectName(u"label_3")
        self.label_3.setText(u"LINK")

        self.gridLayout_2.addWidget(self.label_3, 2, 0, 1, 1)

        self.lineEditCPPArg = QLineEdit(self.groupBoxBuildOptions)
        self.lineEditCPPArg.setObjectName(u"lineEditCPPArg")
        self.lineEditCPPArg.setText(u"")

        self.gridLayout_2.addWidget(self.lineEditCPPArg, 0, 1, 1, 1)


        self.gridLayout_3.addWidget(self.groupBoxBuildOptions, 0, 0, 1, 3)

        self.groupBox = QGroupBox(Dialog)
        self.groupBox.setObjectName(u"groupBox")
        self.groupBox.setTitle(u"")
        self.horizontalLayout_2 = QHBoxLayout(self.groupBox)
        self.horizontalLayout_2.setObjectName(u"horizontalLayout_2")
        self.radioButton = QRadioButton(self.groupBox)
        self.radioButton.setObjectName(u"radioButton")
        self.radioButton.setText(u"Desktop")

        self.horizontalLayout_2.addWidget(self.radioButton)

        self.radioButton_2 = QRadioButton(self.groupBox)
        self.radioButton_2.setObjectName(u"radioButton_2")
        self.radioButton_2.setText(u"Android")

        self.horizontalLayout_2.addWidget(self.radioButton_2)

        self.radioButton_3 = QRadioButton(self.groupBox)
        self.radioButton_3.setObjectName(u"radioButton_3")
        self.radioButton_3.setText(u"Emscripten")

        self.horizontalLayout_2.addWidget(self.radioButton_3)


        self.gridLayout_3.addWidget(self.groupBox, 2, 0, 1, 1)

        self.verticalSpacer_2 = QSpacerItem(20, 5, QSizePolicy.Minimum, QSizePolicy.Minimum)

        self.gridLayout_3.addItem(self.verticalSpacer_2, 1, 2, 1, 1)

        self.groupBox_2 = QGroupBox(Dialog)
        self.groupBox_2.setObjectName(u"groupBox_2")
        self.groupBox_2.setTitle(u"")
        self.horizontalLayout = QHBoxLayout(self.groupBox_2)
        self.horizontalLayout.setObjectName(u"horizontalLayout")
        self.pushButton = QPushButton(self.groupBox_2)
        self.pushButton.setObjectName(u"pushButton")
        self.pushButton.setText(u"Save")

        self.horizontalLayout.addWidget(self.pushButton)

        self.pushButtonCancel = QPushButton(self.groupBox_2)
        self.pushButtonCancel.setObjectName(u"pushButtonCancel")
        self.pushButtonCancel.setText(u"Cancel")

        self.horizontalLayout.addWidget(self.pushButtonCancel)

        self.pushButtonOk = QPushButton(self.groupBox_2)
        self.pushButtonOk.setObjectName(u"pushButtonOk")
        self.pushButtonOk.setText(u"OK")

        self.horizontalLayout.addWidget(self.pushButtonOk)


        self.gridLayout_3.addWidget(self.groupBox_2, 2, 2, 1, 1)

        self.horizontalSpacer_2 = QSpacerItem(40, 20, QSizePolicy.Expanding, QSizePolicy.Minimum)

        self.gridLayout_3.addItem(self.horizontalSpacer_2, 2, 1, 1, 1)
 



        self.pushButtonOk.pressed.connect(self.onSelect)
        self.pushButtonCancel.pressed.connect(self.onCancel)
        self.pushButton.pressed.connect(self.saveOption)
        
        self.radioButton.toggled.connect(lambda:self.switchClick(self.radioButton))
        self.radioButton_2.toggled.connect(lambda:self.switchClick(self.radioButton_2))
        self.radioButton_3.toggled.connect(lambda:self.switchClick(self.radioButton_3))



        QMetaObject.connectSlotsByName(self)
        
        
    def switchClick(self,b):
        if self.radioButton.isChecked():
            self.Plataform=0
        if self.radioButton_2.isChecked():
            self.Plataform=1
        if self.radioButton_3.isChecked():
            self.Plataform=2
      
        self.switchPlatform()
        #self.saveOption()
        self.loadOption()
        
            
    def switchPlatform(self):
        plat = "Dektop"
        if self.Plataform==0:
            self.setWindowTitle(u"Desktop Compiler Options")
        if self.Plataform==1:
            self.setWindowTitle(u"Android Compiler Options")
            plat = "Android"
        if self.Plataform==2:
            self.setWindowTitle(u"Emscripten Compiler Options")
            plat = "Emscripten"
        self.groupBoxBuildOptions.setTitle("Compiler ("+plat+") Options")

        
    
    def loadOption(self):
            args =  None
            if self.Plataform==0:
                args=self.mainWindow.ARGSDESKTOP
            if self.Plataform==1:
                args=self.mainWindow.ARGSANDROID
            if self.Plataform==2:
                args=self.mainWindow.ARGSWEB
 
            self.lineEditLDArg.setText('')
            self.lineEditCCArg.setText('')
            self.lineEditCPPArg.setText('')
            argument=""
            for arg in args.ARGLD:
                argument+=arg+" "
            self.lineEditLDArg.setText(argument)
            argument=""
            for arg in args.ARGCC:
                argument+=arg+" "
            self.lineEditCCArg.setText(argument)
            argument=""
            for arg in args.ARGCPP:
                argument+=arg+" "
            self.lineEditCPPArg.setText(argument)
        
            
        


                    
    def saveOption(self):
        cppArgs = self.lineEditCPPArg.text().strip().split("-")
        ccArgs   = self.lineEditCCArg.text().strip().split("-")
        ldArgs   = self.lineEditLDArg.text().strip().split("-")
        args =  None
        if self.Plataform==0:
            args=self.mainWindow.ARGSDESKTOP
        if self.Plataform==1:
            args=self.mainWindow.ARGSANDROID
        if self.Plataform==2:
            args=self.mainWindow.ARGSWEB
        args.clear()
        for arg in cppArgs:
            value ="-"+arg.strip()
            if len(value)>=2:
                args.addCPP(value)
            
        for arg in ccArgs:
            value ="-"+arg.strip()
            if len(value)>=2:
                args.addCC(value)

        for arg in ldArgs:
            value ="-"+arg.strip()
        if len(value)>=2:
                args.addLD(value)
        
        

        
    def load(self):
        self.switchPlatform()
        self.loadOption()
        if self.Plataform==0:
            self.radioButton.setChecked(True)
        if self.Plataform==1:
            self.radioButton_2.setChecked(True)
        if self.Plataform==2:
            self.radioButton_3.setChecked(True)
        self.show()

        
        
    def onSelect(self):
        self.saveOption()
        self.close()
        
    def onCancel(self):
        self.close()
        
        
class AppModulesWindow(QDialog):
    def __init__(self,parent,plataform):
        super(AppModulesWindow, self).__init__(parent)
        self.mainWindow=parent
        self.Plataform=plataform
        self.project=None
        self.resize(800, 214)
        self.listBoxModule=[]
        sizePolicy = QSizePolicy(QSizePolicy.Maximum, QSizePolicy.Minimum)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(self.sizePolicy().hasHeightForWidth())
        self.setSizePolicy(sizePolicy)
        self.setWindowTitle(u"Add Modules to Aplication")
        self.gridLayout_3 = QGridLayout(self)
        self.gridLayout_3.setObjectName(u"gridLayout_3")
        self.groupBox = QGroupBox(self)
        self.groupBox.setObjectName(u"ModulesgroupBox")
        
        sizePolicy1 = QSizePolicy(QSizePolicy.Expanding, QSizePolicy.Minimum)
        sizePolicy1.setHorizontalStretch(0)
        sizePolicy1.setVerticalStretch(0)
        sizePolicy1.setHeightForWidth(self.groupBox.sizePolicy().hasHeightForWidth())
        
        self.groupBox.setSizePolicy(sizePolicy1)
        self.groupBox.setTitle(u"Modules")
        #self.gridLayout = QGridLayout(self.groupBox)
        #self.gridLayout.setObjectName(u"gridLayout")
        self.gridLayout_3.addWidget(self.groupBox, 0, 0, 1, 3)

        self.moduleGridLayout = QGridLayout(self.groupBox)
        self.moduleGridLayout.setObjectName(u"moduleGridLayout")



        self.verticalSpacer = QSpacerItem(20, 2, QSizePolicy.Minimum, QSizePolicy.MinimumExpanding)

        self.gridLayout_3.addItem(self.verticalSpacer, 1, 1, 1, 1)




        

        self.verticalSpacer_2 = QSpacerItem(20, 5, QSizePolicy.Minimum, QSizePolicy.Minimum)

        self.gridLayout_3.addItem(self.verticalSpacer_2, 3, 2, 1, 1)

        self.horizontalSpacer = QSpacerItem(722, 20, QSizePolicy.Expanding, QSizePolicy.Minimum)

        self.gridLayout_3.addItem(self.horizontalSpacer, 4, 0, 1, 1)

        self.groupBox_2 = QGroupBox(self)
        self.groupBox_2.setObjectName(u"groupBox_2")
        self.groupBox_2.setTitle(u"")
        self.horizontalLayout = QHBoxLayout(self.groupBox_2)
        self.horizontalLayout.setObjectName(u"horizontalLayout")
        self.pushButtonCancel = QPushButton(self.groupBox_2)
        self.pushButtonCancel.setObjectName(u"pushButtonCancel")
        self.pushButtonCancel.setText(u"Cancel")

        self.horizontalLayout.addWidget(self.pushButtonCancel)

        self.pushButtonOk = QPushButton(self.groupBox_2)
        self.pushButtonOk.setObjectName(u"pushButtonOk")
        self.pushButtonOk.setText(u"OK")

        self.horizontalLayout.addWidget(self.pushButtonOk)


        self.gridLayout_3.addWidget(self.groupBox_2, 4, 2, 1, 1)

        self.pushButtonOk.pressed.connect(self.onSelect)
        self.pushButtonCancel.pressed.connect(self.onCancel)


        

        QMetaObject.connectSlotsByName(self)
    def load(self,project):
        self.project=project
        self.mainWindow.loadModules()   
        row = 0
        col = 0
        widgetsPerRow = 8
        index=0
        for module in self.mainWindow.modulesData:
            print("load  ",module.Name)
            checkBoxModule = QCheckBox()
            checkBoxModule.toggled.connect(self.onCheckBoxModule)  
            checkBoxModule.module=module
            checkBoxModule.setObjectName(module.Name)
            if self.project:
                print(self.project.Modules)
                if module.Name in self.project.Modules:
                    checkBoxModule.setChecked(True)
                else:
                    checkBoxModule.setChecked(False)
                
            else:   
                if module.Name in self.mainWindow.GLOBALMODULES:
                    checkBoxModule.setChecked(True)
                else:
                    checkBoxModule.setChecked(False)
                
            
            checkBoxModule.setText(module.Name)
            self.listBoxModule.append(checkBoxModule)
            self.moduleGridLayout.addWidget(checkBoxModule,row,col,1,1)
            index+=1
            col += 1
            if col % widgetsPerRow == 0:
                    row += 1
                    col = 0
        self.show()
    def onCheckBoxModule(self):
        cbutton = self.sender()
        print("module " + (cbutton.module.Name) + " is " + str(cbutton.isChecked()))
        if self.project:
            if cbutton.isChecked():
                if not cbutton.module.Name in self.project.Modules:
                    self.project.Modules.append(cbutton.module.Name)
            else:
                if cbutton.module.Name in self.project.Modules:
                    self.project.Modules.remove(cbutton.module.Name)
        else:
            if cbutton.isChecked():
                if not cbutton.module.Name in self.mainWindow.GLOBALMODULES:
                    self.mainWindow.GLOBALMODULES.append(cbutton.module.Name)
            else:
                if cbutton.module.Name in self.mainWindow.GLOBALMODULES:
                    self.mainWindow.GLOBALMODULES.remove(cbutton.module.Name)
        
        
        
    def onSelect(self):
        if self.project:
            for md in self.project.Modules:
                print(md)
        else:
            for md in self.mainWindow.GLOBALMODULES:
                print(md)
        self.close()
        pass
    def onCancel(self):
        self.close()
        pass

class ModulesWindow(QDialog):
    def __init__(self,parent):
        super(ModulesWindow, self).__init__(parent)
        self.resize(887, 345)
        self.mainWindow=parent
        
        self.listBoxModule=[]
        self.gridLayout = QGridLayout(self)
        self.gridLayout.setObjectName(u"gridLayout")
        self.groupBox = QGroupBox(self)
        self.groupBox.setObjectName(u"groupBox")

        sizePolicy = QSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)
        sizePolicy.setHorizontalStretch(1)
        sizePolicy.setVerticalStretch(1)
        self.groupBox.setSizePolicy(sizePolicy)

        self.moduleGridLayout = QGridLayout(self.groupBox)
        self.moduleGridLayout.setObjectName(u"moduleGridLayout")

        self.gridLayout.addWidget(self.groupBox, 0, 0, 1, 1)

        self.verticalSpacer = QSpacerItem(20, 358, QSizePolicy.Minimum, QSizePolicy.Expanding)

        self.gridLayout.addItem(self.verticalSpacer, 1, 0, 1, 1)

        self.groupBox_2 = QGroupBox(self)
        self.horizontalLayout = QHBoxLayout(self.groupBox_2)
        self.groupBox_3 = QGroupBox(self.groupBox_2)
        self.verticalLayout = QVBoxLayout(self.groupBox_3)
        self.checkBoxDesktop = QCheckBox(self.groupBox_3)
        self.checkBoxDesktop.setChecked(True)
        self.verticalLayout.addWidget(self.checkBoxDesktop)
        self.checkBoxAndroid = QCheckBox(self.groupBox_3)
        self.verticalLayout.addWidget(self.checkBoxAndroid)
        self.checkBoxEmsdk = QCheckBox(self.groupBox_3)
        self.verticalLayout.addWidget(self.checkBoxEmsdk)
        self.horizontalLayout.addWidget(self.groupBox_3)
        
        self.groupBoxAndroidArm = QGroupBox(self.groupBox_2)
        self.groupBoxAndroidArm.setObjectName(u"groupBoxAndroidArm")
        
        self.groupBoxAndroidArm.setTitle(u"Android Arm")
        self.verticalLayout_2 = QVBoxLayout(self.groupBoxAndroidArm)
        self.verticalLayout_2.setObjectName(u"verticalLayout_2")
        self.checkBoxArm7 = QCheckBox(self.groupBoxAndroidArm)
        self.checkBoxArm7.setObjectName(u"checkBoxArm7")
        self.checkBoxArm7.setText(u"ARM7")
        self.checkBoxArm7.setChecked(True)

        self.verticalLayout_2.addWidget(self.checkBoxArm7)

        self.checkBoxArm64 = QCheckBox(self.groupBoxAndroidArm)
        self.checkBoxArm64.setObjectName(u"checkBoxArm64")
        self.checkBoxArm64.setText(u"ARM64")

        self.verticalLayout_2.addWidget(self.checkBoxArm64)


        self.horizontalLayout.addWidget(self.groupBoxAndroidArm)

        
        
        
        self.horizontalSpacer = QSpacerItem(40, 20, QSizePolicy.Expanding, QSizePolicy.Minimum)
        self.horizontalLayout.addItem(self.horizontalSpacer)
        


        self.pushButtonCompileBuild = QPushButton(self.groupBox_2)
        self.pushButtonCompileBuild.setIcon( QIcon(rsrcPath + '/pkg_compile_200.png'))
        self.pushButtonCompileBuild.setText("NormalBuild")
        self.horizontalLayout.addWidget(self.pushButtonCompileBuild)


        self.pushButtonCompile = QPushButton(self.groupBox_2)
        self.pushButtonCompile.setIcon( QIcon(rsrcPath + '/pkg_compile_200.png'))
        self.pushButtonCompile.setText("Compile")
        self.horizontalLayout.addWidget(self.pushButtonCompile)

        self.pushButtonBuild = QPushButton(self.groupBox_2)
        self.pushButtonBuild.setIcon( QIcon(rsrcPath + '/pkg_compile_200.png'))
        self.pushButtonBuild.setText("Build")
        self.horizontalLayout.addWidget(self.pushButtonBuild)


        self.pushButtonClean = QPushButton(self.groupBox_2)
        self.pushButtonClean.setObjectName(u"pushButtonClean")
        self.pushButtonClean.setIcon( QIcon(rsrcPath + '/edit-clear.png'))
        self.horizontalLayout.addWidget(self.pushButtonClean)

        
        self.pushButtonRealod = QPushButton(self.groupBox_2)
        self.pushButtonRealod.setObjectName(u"pushButtonRealod")
        self.pushButtonRealod.setIcon( QIcon(rsrcPath + '/pkg_package_circle_200.png'))
        self.horizontalLayout.addWidget(self.pushButtonRealod)

        self.pushButtonClose = QPushButton(self.groupBox_2)
        self.pushButtonClose.setObjectName(u"pushButtonClose")
        self.pushButtonClose.pressed.connect(self.close)


        self.pushButtonCompile.pressed.connect(self.compile)
        self.pushButtonBuild.pressed.connect(self.build)


        self.pushButtonCompileBuild.pressed.connect(self.compile_and_build)
        self.pushButtonClean.pressed.connect(self.clean)
        self.pushButtonRealod.pressed.connect(self.selectRealod)
        
        
        

        self.horizontalLayout.addWidget(self.pushButtonClose)


        self.gridLayout.addWidget(self.groupBox_2, 2, 0, 1, 1)


        

        QMetaObject.connectSlotsByName(self)
        self.retranslateUi()

    def selectRealod(self):
        print("reload modules")
        for w in self.listBoxModule:
            self.moduleGridLayout.removeWidget(w)
            w.close()
            w=None

        self.listBoxModule.clear()
        self.mainWindow.loadModules(True)
        row = 0
        col = 0
        widgetsPerRow = 8
        index=0
        for module in self.mainWindow.modulesData:
            print("load ",module.Name)
            checkBoxModule = QCheckBox()
            checkBoxModule.toggled.connect(self.onCheckBoxModule)  
            checkBoxModule.setObjectName(module.Name)
            checkBoxModule.setChecked(False)
            checkBoxModule.module=module
            checkBoxModule.setText(module.Name)
            self.listBoxModule.append(checkBoxModule)
            self.moduleGridLayout.addWidget(checkBoxModule,row,col,1,1)
            index+=1
            col += 1
            if col % widgetsPerRow == 0:
                    row += 1
                    col = 0

    def clean(self):
        compileDesktop =self.checkBoxDesktop.isChecked()
        compileAndroid =self.checkBoxAndroid.isChecked()
        compileWeb     =self.checkBoxEmsdk.isChecked()
       
        for cbutton in self.listBoxModule:
            if (cbutton.isChecked()):
                module = cbutton.module
                module.clean()
    def compile(self):
        compileDesktop =self.checkBoxDesktop.isChecked()
        compileAndroid =self.checkBoxAndroid.isChecked()
        compileWeb     =self.checkBoxEmsdk.isChecked()
        for cbutton in self.listBoxModule:
            if (cbutton.isChecked()):
                module = cbutton.module
                if compileDesktop:
                    module.compileDesktop()
                if compileAndroid:
                    if self.checkBoxArm7.isChecked()==False and self.checkBoxArm64.isChecked()==False:
                        self.checkBoxArm7.setChecked(True)
                        
                    module.compileAndroid(self.checkBoxArm7.isChecked(),self.checkBoxArm64.isChecked())
                if compileWeb:
                    module.compileWeb()
    def build(self):
        compileDesktop =self.checkBoxDesktop.isChecked()
        compileAndroid =self.checkBoxAndroid.isChecked()
        compileWeb     =self.checkBoxEmsdk.isChecked()
        for cbutton in self.listBoxModule:
            if (cbutton.isChecked()):
                module = cbutton.module
                if compileDesktop:
                    module.buildDesktop()
                if compileAndroid:
                    if self.checkBoxArm7.isChecked()==False and self.checkBoxArm64.isChecked()==False:
                        self.checkBoxArm7.setChecked(True)
                    module.buildAndroid(self.checkBoxArm7.isChecked(),self.checkBoxArm64.isChecked())
                if compileWeb:
                    module.buildWeb()             

    def compile_and_build(self):
        compileDesktop =self.checkBoxDesktop.isChecked()
        compileAndroid =self.checkBoxAndroid.isChecked()
        compileWeb     =self.checkBoxEmsdk.isChecked()
        for cbutton in self.listBoxModule:
            if (cbutton.isChecked()):
                module = cbutton.module
                if compileDesktop:
                    module.compileDesktop()
                if compileAndroid:
                    if self.checkBoxArm7.isChecked()==False and self.checkBoxArm64.isChecked()==False:
                        self.checkBoxArm7.setChecked(True)
                    module.compileAndroid(self.checkBoxArm7.isChecked(),self.checkBoxArm64.isChecked())
                if compileWeb:
                    module.compileWeb()


    def onCheckBoxModule(self):
        cbutton = self.sender()
        #print("module " + (cbutton.module.name) + " is " + str(cbutton.isChecked()))
        pass

   

           
    def load(self):
        self.mainWindow.loadModules()
        row = 0
        col = 0
        widgetsPerRow = 8
        index=0
        for module in self.mainWindow.modulesData:
            print("load ",module.Name)
            checkBoxModule = QCheckBox()
            checkBoxModule.toggled.connect(self.onCheckBoxModule)  
            checkBoxModule.setObjectName(module.Name)
            checkBoxModule.setChecked(False)
            checkBoxModule.module=module
            checkBoxModule.setText(module.Name)
            self.listBoxModule.append(checkBoxModule)
            self.moduleGridLayout.addWidget(checkBoxModule,row,col,1,1)
            index+=1
            col += 1
            if col % widgetsPerRow == 0:
                    row += 1
                    col = 0
            
        self.show()
        pass

    def retranslateUi(self):
        self.setWindowTitle(QCoreApplication.translate("Dialog", u"Dialog", None))
        
        self.groupBox.setTitle(QCoreApplication.translate("Dialog", u"Modules", None))
        self.groupBox_2.setTitle(QCoreApplication.translate("Dialog", u"Actions", None))
        self.groupBox_3.setTitle(QCoreApplication.translate("Dialog", u"Plataform", None))
        self.checkBoxDesktop.setText(QCoreApplication.translate("Dialog", u"Desktop", None))
        self.checkBoxAndroid.setText(QCoreApplication.translate("Dialog", u"Android", None))
        self.checkBoxEmsdk.setText(QCoreApplication.translate("Dialog", u"Emscripten", None))
        
        self.pushButtonClean.setText(QCoreApplication.translate("Dialog", u"Clean", None))
        
        self.pushButtonRealod.setText(QCoreApplication.translate("Dialog", u"Reload", None))
        self.pushButtonClose.setText(QCoreApplication.translate("Dialog", u"Close", None))    # retranslateUi



class Theme():

    class LexerCPP(QsciLexerCPP):

        def __init__(self, parent=None):
            super().__init__(parent)

        def defaultColor(self, style):
            DEFAULT = QColor(247, 247, 241)
            KEYWORD = QColor(249, 38, 114)
            DATATYPE = QColor(102, 216, 238)
            NUMBER = QColor(174, 129, 255)
            OPERATOR = QColor(249, 38, 114)
            STRING = QColor(255, 219, 116)
            FUNCTION = QColor(166, 226, 46)
            COMMENT = QColor(117, 113, 94)
            HASHCOMMENT = QColor(174, 129, 255)

            dct = {
                self.Comment: COMMENT,
                self.CommentLine: COMMENT,
                self.CommentDoc: COMMENT,
                self.CommentLineDoc: COMMENT,
                self.PreProcessorCommentLineDoc: COMMENT,
                self.Number: NUMBER,
                self.Keyword: FUNCTION,
                # self.Keyword: KEYWORD,
                self.KeywordSet2: QColor(102, 216, 238),
                self.DoubleQuotedString: STRING,
                self.SingleQuotedString: STRING,
                self.RawString: STRING,
                self.PreProcessor: QColor(0x7f, 0x7f, 0x00),
                self.Operator: DEFAULT,
                self.UnclosedString: STRING,
                self.VerbatimString: STRING,
                self.TripleQuotedVerbatimString: STRING,
                self.HashQuotedString: STRING,
                self.Regex: QColor(0x3f, 0x7f, 0x3f),
                self.CommentDocKeyword: QColor(0x30, 0x60, 0xa0),
                self.CommentDocKeywordError: QColor(0x80, 0x40, 0x20),
                self.PreProcessorComment: QColor(0x65, 0x99, 0x00),
                self.InactiveDefault: QColor(0xc0, 0xc0, 0xc0),
                self.InactiveUUID: QColor(0xc0, 0xc0, 0xc0),
                self.InactiveCommentLineDoc: QColor(0xc0, 0xc0, 0xc0),
                self.InactiveKeywordSet2: QColor(0xc0, 0xc0, 0xc0),
                self.InactiveCommentDocKeyword: QColor(0xc0, 0xc0, 0xc0),
                self.InactiveCommentDocKeywordError: QColor(0xc0, 0xc0, 0xc0),
                self.InactivePreProcessorCommentLineDoc: QColor(0xc0, 0xc0, 0xc0),
                self.InactiveComment: QColor(0x90, 0xb0, 0x90),
                self.InactiveCommentLine: QColor(0x90, 0xb0, 0x90),
                self.InactiveNumber: QColor(174, 129, 255),
                self.InactiveVerbatimString: STRING,
                self.InactiveTripleQuotedVerbatimString: STRING,
                self.InactiveHashQuotedString: QColor(0x90, 0xb0, 0x90),
                self.InactiveCommentDoc: QColor(0xd0, 0xd0, 0xd0),
                self.InactiveKeyword: QColor(0x90, 0x90, 0xb0),
                self.InactiveDoubleQuotedString: STRING,
                self.InactiveSingleQuotedString: STRING,
                self.InactiveRawString: STRING,
                self.InactivePreProcessor: QColor(0xb0, 0xb0, 0x90),
                self.InactiveOperator: QColor(0xb0, 0xb0, 0xb0),
                self.InactiveIdentifier: QColor(0xb0, 0xb0, 0xb0),
                self.InactiveGlobalClass: QColor(0xb0, 0xb0, 0xb0),
                self.InactiveUnclosedString: STRING,
                self.InactiveRegex: QColor(0x7f, 0xaf, 0x7f),
                self.InactivePreProcessorComment: QColor(0xa0, 0xc0, 0x90),
                self.UserLiteral: QColor(0xc0, 0x60, 0x00),
                self.InactiveUserLiteral: QColor(0xd7, 0xa0, 0x90),
                self.TaskMarker: QColor(0xbe, 0x07, 0xff),
                self.InactiveTaskMarker: QColor(0xc3, 0xa1, 0xcf)
            }

            return dct.get(style, DEFAULT)

        def defaultPaper(self, style):
            # return self.parent().paper()
            return QColor(39, 40, 34)

        def defaultFont(self, style):
            font = QFont()
            font.setFamily('Consolas')
            font.setFixedPitch(True)
            font.setPointSize(12)
            font.setBold(False)
            return font

    def __init__(self, sci):
        self.sci = sci

        sci.setFolding(QsciScintilla.CircledFoldStyle)
        sci.setMarginType(1, QsciScintilla.NumberMargin)
        sci.setMarginSensitivity(1, True)
        sci.markerDefine(QsciScintilla.RightArrow, 8)
        sci.setMarkerBackgroundColor(QColor("#ee1111"), 8)

        lexer = self.LexerCPP()
        lexer.setFoldAtElse(True)
        lexer.setFoldComments(True)
        lexer.setFoldCompact(False)
        lexer.setFoldPreprocessor(True)
        sci.setLexer(lexer)


class Monokai():

    def __init__(self, sci):
        self.sci = sci

        # Set default font
        sci.font = QFont()
        sci.font.setFamily('Consolas')
        sci.font.setFixedPitch(True)
        sci.font.setPointSize(8)
        sci.font.setBold(True)
        sci.setFont(sci.font)
        sci.setMarginsFont(sci.font)
        sci.setUtf8(True)

        # Set paper
        sci.setPaper(QColor(39, 40, 34))

        # Set margin defaults
        fontmetrics = QFontMetrics(sci.font)
        sci.setMarginsFont(sci.font)
        sci.setMarginWidth(0, fontmetrics.width("000") + 6)
        sci.setMarginLineNumbers(0, True)
        sci.setMarginsForegroundColor(QColor(128, 128, 128))
        sci.setMarginsBackgroundColor(QColor(39, 40, 34))
        sci.setMarginType(1, sci.SymbolMargin)
        sci.setMarginWidth(1, 12)
        # sci.setMarginMarkerMask(1,
        #                   1 << MarkerType::ERROR |
        #                   1 << MarkerType::WARNING
        #                   );

        # Set indentation defaults
        sci.setIndentationsUseTabs(False)
        sci.setIndentationWidth(4)
        sci.setBackspaceUnindents(True)
        sci.setIndentationGuides(True)
        # sci.setIndentationGuidesBackgroundColor(QColor("#2a2b24"))
        # sci.setIndentationGuidesForegroundColor(QColor("#2a2b24"))
        sci.setFoldMarginColors(QColor(39, 40, 34), QColor(39, 40, 34))
        # sci.setEolMode(QsciScintilla.EolUnix)

        # Set caret defaults
        sci.setCaretForegroundColor(QColor(247, 247, 241))
        sci.setCaretWidth(2)

        # Set edge defaults
        sci.setEdgeColumn(80)
        sci.setEdgeColor(QColor('#dddddd'))
        sci.setEdgeMode(sci.EdgeLine)

        # Set folding defaults (http://www.scintilla.org/ScintillaDoc.html#Folding)
        sci.setFolding(QsciScintilla.BoxedFoldStyle)
        #PlainFoldStyle
        #CircledFoldStyle
        #BoxedFoldStyle
        #CircledTreeFoldStyle
        #BoxedTreeFoldStyle
        

        # Set brace defaults
        # sci.setBraceMatching(sci.SloppyBraceMatch)
        # sci.setMatchedBraceBackgroundColor(QColor(62, 61, 50))
        # sci.setUnmatchedBraceBackgroundColor(QColor(249, 38, 114))

        sci.setWrapMode(sci.WrapNone)

        # Set selection color defaults
        sci.setSelectionBackgroundColor(QColor(61, 61, 52))
        sci.resetSelectionForegroundColor()

        # Set scrollwidth defaults
        sci.SendScintilla(QsciScintilla.SCI_SETSCROLLWIDTHTRACKING, 1)

        # Current line visible with special background color
        # sci.setCaretLineVisible(True)
        # sci.setCaretLineBackgroundColor(QColor('#ffffe0'))

        # Set multiselection defaults
        sci.SendScintilla(QsciScintilla.SCI_SETMULTIPLESELECTION, True)
        sci.SendScintilla(QsciScintilla.SCI_SETMULTIPASTE, 1)
        sci.SendScintilla(QsciScintilla.SCI_SETADDITIONALSELECTIONTYPING, True)
  

class CodeEditor(QsciScintilla):
    comment_string = "// "
    line_ending = "\n"
    def __init__(self,FilesList, parent=None):
        QsciScintilla.__init__(self, parent)

        self.setup()
        self.mainWindow=parent
        self.project=None#Project(self.mainWindow)
        self.currentLine=0
        self.currentCursor=0
        self.PACKAGE=""        
        self.ACTIVITY=""
        self.LABEL=""
        self.SHELL=""
        
        self.IsProject=False
        self.FilesList=FilesList
        features = [
        Theme(self),
        Monokai(self)    ]
        self.setup()
        self.cursorPositionChanged.connect(self.OncursorPositionChanged)

        
    def enterEvent(self, event):
        #print("is project :",self.IsProject)
        if self.project:
            #print("Project Name",self.project.Name)
            self.mainWindow.actionSaveProject.setEnabled(True)
            self.mainWindow.setCurrentFileName()
        else:
            self.mainWindow.actionSaveProject.setEnabled(False)
            #print("editore  not project")
        
        

    def leaveEvent(self, event):
        pass

    def showEvent(self, event):
        self.FilesList.clearAll()
        self.FilesList.loadProject(self.project)
        
        
        
        if self.project:
            print(self.project.Name)
            self.mainWindow.actionSaveProject.setEnabled(True)
            
        else:
            self.mainWindow.actionSaveProject.setEnabled(False)
            
                
        
        
        
       
        
    def OncursorPositionChanged(self,line,index):
        #print(line," ",index)
        self.currentLine=line
        self.currentCursor=index
        self.mainWindow.setStatusBarLineCount(line)
    def saveProject(self):
        print("save project")
    def loadProject(self):
        print("save project")
    def compileProject(self):
        print("compile project")
    def buildProject(self):
        print("build project")
    def runProject(self):
        print("run project")

    def keyPressEvent(self, event):
        # Execute the superclasses event
        super().keyPressEvent(event)
        # Check pressed key information
        key = event.key()
        key_modifiers = QApplication.keyboardModifiers()
        if (key == QtCore.Qt.Key_K and 
            key_modifiers == QtCore.Qt.ControlModifier):
                self.toggle_commenting()
    
    def toggle_commenting(self):
        # Check if the selections are valid
        selections = self.get_selections()
        if selections == None:
            return
        # Merge overlapping selections
        while self.merge_test(selections) == True:
            selections = self.merge_selections(selections)
        # Start the undo action that can undo all commenting at once
        self.beginUndoAction()
        # Loop over selections and comment them
        for i, sel in enumerate(selections):
            if self.text(sel[0]).lstrip().startswith(self.comment_string):
                self.set_commenting(sel[0], sel[1], self._uncomment)
            else:
                self.set_commenting(sel[0], sel[1], self._comment)
        # Select back the previously selected regions
        self.SendScintilla(self.SCI_CLEARSELECTIONS)
        for i, sel in enumerate(selections):
            start_index = self.positionFromLineIndex(sel[0], 0)
            # Check if ending line is the last line in the editor
            last_line = sel[1]
            if last_line == self.lines() - 1:
                end_index = self.positionFromLineIndex(sel[1], len(self.text(last_line)))
            else:
                end_index = self.positionFromLineIndex(sel[1], len(self.text(last_line))-1)
            if i == 0:
                self.SendScintilla(self.SCI_SETSELECTION, start_index, end_index)
            else:
                self.SendScintilla(self.SCI_ADDSELECTION, start_index, end_index)
        # Set the end of the undo action
        self.endUndoAction()
    
    def get_selections(self):
        # Get the selection and store them in a list
        selections = []
        for i in range(self.SendScintilla(self.SCI_GETSELECTIONS)):
            selection = (
                self.SendScintilla(self.SCI_GETSELECTIONNSTART, i),
                self.SendScintilla(self.SCI_GETSELECTIONNEND, i)
            )
            # Add selection to list
            from_line, from_index = self.lineIndexFromPosition(selection[0])
            to_line, to_index = self.lineIndexFromPosition(selection[1])
            selections.append((from_line, to_line))
        selections.sort()
        # Return selection list
        return selections
    
    def merge_test(self, selections):
        """
        Test if merging of selections is needed
        """
        for i in range(1, len(selections)):
            # Get the line numbers
            previous_start_line = selections[i-1][0]
            previous_end_line = selections[i-1][1]
            current_start_line = selections[i][0]
            current_end_line = selections[i][1]
            if previous_end_line == current_start_line:
                return True
        # Merging is not needed
        return False
    
    def merge_selections(self, selections):
        """
        This function merges selections with overlapping lines
        """
        # Test if merging is required
        if len(selections) < 2:
            return selections
        merged_selections = []
        skip_flag = False
        for i in range(1, len(selections)):
            # Get the line numbers
            previous_start_line = selections[i-1][0]
            previous_end_line = selections[i-1][1]
            current_start_line = selections[i][0]
            current_end_line = selections[i][1]
            # Test for merge
            if previous_end_line == current_start_line and skip_flag == False:
                merged_selections.append(
                    (previous_start_line, current_end_line)
                )
                skip_flag = True
            else:
                if skip_flag == False:
                    merged_selections.append(
                        (previous_start_line, previous_end_line)
                    )
                skip_flag = False
                # Add the last selection only if it was not merged
                if i == (len(selections) - 1):
                    merged_selections.append(
                        (current_start_line, current_end_line)
                    )
        # Return the merged selections
        return merged_selections
    
    def set_commenting(self, arg_from_line, arg_to_line, func):
        # Get the cursor information
        from_line = arg_from_line
        to_line = arg_to_line
        # Check if ending line is the last line in the editor
        last_line = to_line
        if last_line == self.lines() - 1:
            to_index = len(self.text(to_line))
        else:
            to_index = len(self.text(to_line))-1
        # Set the selection from the beginning of the cursor line
        # to the end of the last selection line
        self.setSelection(
            from_line, 0, to_line, to_index
        )
        # Get the selected text and split it into lines
        selected_text = self.selectedText()
        selected_list = selected_text.split("\n")
        # Find the smallest indent level
        indent_levels = []
        for line in selected_list:
            indent_levels.append(len(line) - len(line.lstrip()))
        min_indent_level = min(indent_levels)
        # Add the commenting character to every line
        for i, line in enumerate(selected_list):
            selected_list[i] = func(line, min_indent_level)
        # Replace the whole selected text with the merged lines
        # containing the commenting characters
        replace_text = self.line_ending.join(selected_list)
        self.replaceSelectedText(replace_text)
    
    def _comment(self, line, indent_level):
        if line.strip() != "":
            return line[:indent_level] + self.comment_string + line[indent_level:]
        else:
            return line
    
    def _uncomment(self, line, indent_level):
        if line.strip().startswith(self.comment_string):
            return line.replace(self.comment_string, "", 1)
        else:
            return line
    def setup(self):
        # Set the autocompletion to be case IN-sensitive
        self.setAutoCompletionCaseSensitivity(False)
        # Set the threshold at which the autocompletion window appears
        self.setAutoCompletionThreshold(1)
        # Set the source from which the autocompletions will be pulled from
        self.setAutoCompletionSource(QsciScintilla.AcsAll)
        # Sets whether the characters to the right of the autocompletion
        # will be overwritten when an autocompletion is selected.
        self.setAutoCompletionReplaceWord(True)
        # Select the behaviour of autocompletions when there is only a single
        # entry in the autocompletion list. The selection below sets that
        # when the autocompletion window will always be displayed.
        self.setAutoCompletionUseSingle(QsciScintilla.AcusNever)
        
        self.setMarginType(1, QsciScintilla.NumberMargin)
        self.SendScintilla(QsciScintilla.SCI_SETMULTIPLESELECTION, 1)
        self.SendScintilla(QsciScintilla.SCI_SETADDITIONALSELECTIONTYPING, True)
        

        self.setCallTipsStyle(QsciScintilla.CallTipsNoContext)
        self.setCallTipsVisible(0)
        self.setCallTipsPosition(QsciScintilla.CallTipsAboveText)
        self.setCallTipsBackgroundColor(QColor(0xff, 0xff, 0xff, 0xff))
        self.setCallTipsForegroundColor(QColor(0x50, 0x50, 0x50, 0xff))
        self.setCallTipsHighlightColor(QColor(0xff, 0x00, 0x00, 0xff))
        self.setCaretForegroundColor(QColor(0xff, 0x00, 0x00, 0x00))
        self.setCaretLineVisible(True)
        self.setCaretLineBackgroundColor(QColor('lightblue'))
        

class CodeListView(QListWidget):
    
    def __init__(self,  parent=None):
        super(CodeListView, self).__init__(parent)
        self.setAcceptDrops(True)
        self.setIconSize(QtCore.QSize(72, 72))
        self.items = []
        self.mainfolder = None
        self.mainWindow = parent
        self.project=None
        
        
        self.clicked.connect(self.onClick)
        self.doubleClicked.connect(self.onDoubleClicked)
    

    def loadProject(self,project):
        self.clearAll()
        if project:
            self.project=project
            for src in project.Src.Src:
                self.addSrcFile(src)
        else:
            self.project=None
            

    def onClick(self):
        #print("clink")    
        pass

    def onDoubleClicked(self,item):
        #print("doublw clink",str(item.row())) 
        #print("doublw clink",str(item.row()))   
        #print(self.items[item.row()])
        #self.mainWindow.load(self.items[item.row()])  
        self.mainWindow.load(self.items[item.row()])   
        pass


    def contextMenuEvent(self, event):
        contextMenu = QMenu(self)
        clrAct = contextMenu.addAction("Clear")
        removeAct = contextMenu.addAction("Remove")
        listAct = contextMenu.addAction("List")
        addAct = contextMenu.addAction("Add")

        if self.count()<=0:
            return 
        
        
        
        #print("count ",self.count())
        #print("row ",self.currentRow())
        index = self.currentRow()
        

        action = contextMenu.exec_(self.mapToGlobal(event.pos()))
        if action == clrAct:
            self.items.clear()
            self.clear()
            if self.project:
                self.project.removeCode()
                self.project.save(None)
                
            self.mainfolder==None
        if action == removeAct:
            print("Remove   ",str(self.items[index])," index: ", index)
            if self.project:
                self.project.removeSrc(str(self.items[index]))
                self.project.save(None)
            self.items.pop(index)
            self.takeItem(index)
        if action==listAct:
            for i in range(self.count()):
                print(self.items[i])
        if action==addAct:
            fn, _ = QFileDialog.getOpenFileName(self, "Open File...", None,
                "Code files (*.cpp *.c);;All Files (*)")
            if fn:
                self.addSrcFile(fn)
                if self.project:
                    self.project.addSrc(fn)
                    self.project.save(None)

            
    def clearAll(self):
        self.items=[]
        self.clear()
    def MoveUp(self):
        currentRow = self.currentRow()
        currentItem = self.takeItem(currentRow)
        self.insertItem(currentRow - 1, currentItem)

    def MoveDown(self):
        currentRow = self.currentRow()
        currentItem = self.takeItem(currentRow)
        self.insertItem(currentRow + 1, currentItem)        

    def dragEnterEvent(self, event):
        if event.mimeData().hasUrls:
            event.accept()
        else:
            event.ignore()

    def dragMoveEvent(self, event):
        if event.mimeData().hasUrls:
            event.setDropAction(QtCore.Qt.CopyAction)
            event.accept()
        else:
            event.ignore()
    
    def addSrcFile(self,src):
        if not src in self.items:
            filename, file_extension = os.path.splitext(src)
            self.items.append(src)
            self.addItem(os.path.basename(filename))
            #if self.project:
            #    self.project.addSrc(src)
    
    

    def dropEvent(self, event):
        if event.mimeData().hasUrls:
            event.setDropAction(QtCore.Qt.CopyAction)
            event.accept()
            for url in event.mimeData().urls():
                isDirectory = os.path.isdir(str(url.toLocalFile()))
                if isDirectory:
                    path = str(url.toLocalFile())
                    for root, dirs, files in os.walk(path):
                        for file in files:
                            ext = os.path.splitext(file)[1]
                            print(os.path.join(root, file))
                            self.addSrcFile(os.path.join(root, file))
                            if self.project:
                                self.project.addSrc(os.path.join(root, file))
                            
                else:
                    self.addSrcFile(str(url.toLocalFile()))
                    if self.project:
                        self.project.addSrc(str(url.toLocalFile()))
            
            if self.project:
                self.project.save(None)
        

        else:
            event.ignore()
    




    def stop(self):
        self.wait()    
class MainWindow(QMainWindow):
    def __init__(self):
        super(MainWindow, self).__init__()
        self.setToolButtonStyle(Qt.ToolButtonFollowStyle)
        
        self.runner=None
        self.projectCompiler=None
        self.setupFileActions()
        self.setupProjectActions()

        self.setupCompilerActions()
        self.setupModulesActions()

        self.setupEditActions()        
        
        self.setupAndroidActions()
        
        self.modulesList=[]
        self.modulesData=[]
        self.currentPlataform=0
        
        self.IsDone=False
        self.ARGSDESKTOP=Args()
        self.ARGSANDROID=Args()
        self.ARGSWEB=Args()
        #self.GLOBALARGS.ARGCC.append("-std=c99")
        #self.GLOBALARGS.ARGCPP.append("-std=c++11")
        self.GLOBALMODULES=[]
        
        self.LAST_DIR=os.getcwd()+"/projects"
        self.LAST_PROJECT=""
        self.projects=[]
        self.files=[]

        self.statusbar = QStatusBar(self)
        self.setStatusBar(self.statusbar)
        self.lineCountLabel = QLabel("Current Line: 1")
        self.statusbar.addPermanentWidget(self.lineCountLabel)

        self.dock = QDockWidget()
        self.dock.setWindowTitle('Project Src List')
        self.listSrc = CodeListView(self)
        self.dock.setWidget(self.listSrc)
        self.listSrc.show()
        self.addDockWidget(Qt.LeftDockWidgetArea, self.dock)
        
        
        self.Consoledock = QDockWidget()
        self.Consoledock.setWindowTitle('Console')
        self.MainConsole = ConsoleText()
        self.MainConsole.appendPlainText("By DjokerSoft @2021")
        self.Consoledock.setWidget(self.MainConsole)
        
        self.addDockWidget(Qt.BottomDockWidgetArea, self.Consoledock)
      
        
        
        
        self.setWindowTitle(self.tr("%s[*] - %s" % ("editor", "Cross Compile IDE")))
        self.setWindowModified(False)
        
        helpMenu = QMenu("Help", self)
        self.menuBar().addMenu(helpMenu)
        helpMenu.addAction("About", self.about)
        helpMenu.addAction("About &Qt", QApplication.instance().aboutQt)
 
        
        QApplication.clipboard().dataChanged.connect(self.clipboardDataChanged)

        self.cpplexer = QsciLexerCPP()
        self.api = QsciAPIs(self.cpplexer)

        for root, directories, files in os.walk(os.getcwd()+os.path.sep+"api", topdown=False):
            for name in files:
                ext = os.path.splitext(name)[1]
                if (ext==".api"):
                    print(os.path.join(root, name)," ",ext," ",directories)
                    with open(os.path.join(root, name)) as data:
                        for rline in data.readlines():
                            line=rline.strip()
                            if (len(line))>=1:
                                self.api.add(line)
                            
                            

                    #self.api.add(line)
        
        self.api.prepare()
                    
    
        
        #wordlist = os.getcwd()+os.path.sep+"res"+os.path.sep+"wordlist.txt"

        #self.AutoCompletionsFromFile(wordlist)


        self.X=0
        self.Y=0
        self.codeTab = QTabWidget()
        self.codeTab.setTabBarAutoHide(False)
        self.codeTab.setMovable(True)
        self.codeTab.setTabsClosable(True)
        self.codeTab.tabCloseRequested.connect(self.onCloseTab)
        self.codeTab.currentChanged.connect(self.onChangeTab)
        
        self.codeTab.tabBarClicked.connect(self.onCickTab)
        self.codeTab.tabBarDoubleClicked.connect(self.onDblClickTab)
        
        
        
        #self.pushButtonCancel.pressed.connect(self.onCancel)
        #connect(tabBar, &QTabBar::tabCloseRequested, this, &TabWidget::closeTab);
        

        #newAction.triggered.connect(self.openDialog)
        #quitAction.triggered.connect(self.close)

        self.setCentralWidget(self.codeTab)
        
        try:        
            with open("config.json", 'r') as data:
                jdata = json.load(data)
                values = jdata["Configuration"]
                files=values["Files"]
                for f in files:
                    self.load(str(f))
                projects=values["Projects"]
                for proj in projects:
                    self.loadProject(str(proj))
                
                modules=values["Modules"]
                for module in modules:
                    self.GLOBALMODULES.append(module)
                    
                platArgs=values['Desktop']
                ArgsCPP=platArgs["ArgCPP"]
                ArgsCC =platArgs["ArgCC"]
                ArgsLD =platArgs["ArgLD"]
               
                for arg in ArgsCPP:
                    self.ARGSDESKTOP.addCPP(str(arg))
                
                for arg in ArgsCC:
                    self.ARGSDESKTOP.addCC(str(arg))

                for arg in ArgsLD:
                    self.ARGSDESKTOP.addLD(str(arg))
                    
                platArgs=values['Android']
                ArgsCPP=platArgs["ArgCPP"]
                ArgsCC =platArgs["ArgCC"]
                ArgsLD =platArgs["ArgLD"]
               
                for arg in ArgsCPP:
                    self.ARGSANDROID.addCPP(str(arg))
                
                for arg in ArgsCC:
                    self.ARGSANDROID.addCC(str(arg))

                for arg in ArgsLD:
                    self.ARGSANDROID.addLD(str(arg))                
                
                platArgs=values['Web']
                ArgsCPP=platArgs["ArgCPP"]
                ArgsCC =platArgs["ArgCC"]
                ArgsLD =platArgs["ArgLD"]
               
                for arg in ArgsCPP:
                    self.ARGSWEB.addCPP(str(arg))
                
                for arg in ArgsCC:
                    self.ARGSWEB.addCC(str(arg))

                for arg in ArgsLD:
                    self.ARGSWEB.addLD(str(arg))
        except Exception as e:
            print('Error Load Settings  , on line {}'.format(sys.exc_info()[-1].tb_lineno), type(e).__name__, e)

        
        
        self.loadModules()       
        



        
    def setStatusBarLineCount(self,count):
        self.lineCountLabel.setText("Current Line: "+str(count))

    def onChangeTab(self,index):
        #self.listSrc.clearAll()
        textEdit = self.codeTab.currentWidget()
        if textEdit:
            project = textEdit.project
            if project:
                #print("load files")
                #self.listSrc.loadProject(project)
                if textEdit.IsProject:
                    self.actionSaveProject.setEnabled(True)
            else:
                 self.actionSaveProject.setEnabled(False)
        else:
            self.actionSaveProject.setEnabled(False)
                
        
        self.setCurrentFileName()
        
    def onCickTab(self,index):
        pass
    def onDblClickTab(self,index):
        pass
        

    def onCloseTab(self,index):
        self.fileSave()
        textEdit = self.codeTab.currentWidget()
        textEdit.close()
        if textEdit.project in self.projects:
            self.projects.remove(textEdit.project)
        if textEdit.fileName in self.files:
            self.files.remove(textEdit.fileName)
            
        self.codeTab.removeTab(index)
        if self.codeTab.count()==0:
           self.actionSaveAs.setEnabled(False)
           self.actionSave.setEnabled(False)
           self.actionSaveProject.setEnabled(False)
           self.LAST_PROJECT=None

    def log(self,msg):
        self.MainConsole.appendPlainText(msg)        

    def trace(self,*args):
        result = ""
        for x in args:
            result += str(x)
        
        self.MainConsole.appendPlainText(result)

    def ClearConsole(self):
        self.MainConsole.clear()
        
                    
    def  AutoCompletionsFromFile(self, fileName):
        f = QFile(fileName)
        if not f.open(QFile.ReadOnly):
            print("erro")
            return 

        QApplication.setOverrideCursor(QCursor(Qt.WaitCursor))

        words = []
        while not f.atEnd():
            line = f.readLine().trimmed()
            if line.length() != 0:
                try:
                    line = str(line, encoding='ascii')
                except TypeError:
                    line = str(line)
                #print(line)
                self.api.add(line)
                words.append(line)
        
                
        
        self.api.prepare()
        QApplication.restoreOverrideCursor()
    
    def selectWorkPlataform(self):
        pass
    def addModule(self):
        AppModules = AppModulesWindow(self,self.currentPlataform)
        AppModules.load(None)
    def addModuleToProject(self):
        AppModules = AppModulesWindow(self,self.currentPlataform)
        textEdit = self.codeTab.currentWidget()
        AppModules.load(textEdit.project)
    def compileOptions(self):
        AppOptions = CompileOptionsWindow(self,self.currentPlataform)
        AppOptions.load()
        
    def switchPlataform(self,index):
        self.currentPlataform=index
        print(self.currentPlataform)

    def setupAndroidActions(self):
        menu = QMenu("&Android", self)
        self.menuBar().addMenu(menu)
        #actionTemplate = QAction( "&Template", self, priority=QAction.LowPriority, triggered=self.addTemplate)
        
        submenu = QMenu('Template',self)
        submenu.addAction(QAction( "Native", self, priority=QAction.LowPriority, triggered=self.addNativeTemplate))
        submenu.addAction(QAction( "SDL2", self, priority=QAction.LowPriority, triggered=self.addSDL2Template))
        
        
        menu.addMenu(submenu)
        
        
        #menu.addAction(actionTemplate)

    def addNativeTemplate(self):
        print("native")
        self.fileSave()
        textEdit = self.codeTab.currentWidget()
        
        if not textEdit:
            return 
            
        project =textEdit.project
        FileName = textEdit.fileName
        
        Name=os.path.splitext(os.path.basename(FileName))[0]
        Dir=os.path.dirname(os.path.abspath(FileName))
        filename, file_extension = os.path.splitext(FileName)
        
        print(Name)
        print(Dir)
        print(filename)
        print(FileName)
        
                
        if project:
            self.trace("Use Project Options")
        else:
            pass
            
            
        outFolder=Dir+OSP+"Android"+OSP+Name+OSP
        print(outFolder)
        createFolderTree(outFolder)
        res = outFolder+"res"
        if not os.path.exists(res):
            self.trace("*** Create :"+res)
            os.mkdir(res)
        
        resPath = os.getcwd()+os.path.sep+"Templates"+os.path.sep+'Android'+os.path.sep+"Res"    
        res = outFolder+"res"+OSP+'mipmap-hdpi'
        if not os.path.exists(res):
            os.mkdir(res)
        
        if not os.path.exists(res+OSP+"ic_launcher.png"):    
            shutil.copy(resPath+OSP+'mipmap-hdpi'+OSP+'ic_launcher.png',res)
            
        res = outFolder+"res"+OSP+'mipmap-mdpi'
        if not os.path.exists(res):
            os.mkdir(res)
        if not os.path.exists(res+OSP+"ic_launcher.png"):    
            shutil.copy(resPath+OSP+'mipmap-hdpi'+OSP+'ic_launcher.png',res)


        res = outFolder+"res"+OSP+'mipmap-xhdpi'
        if not os.path.exists(res):
            os.mkdir(res)
        if not os.path.exists(res+OSP+"ic_launcher.png"):    
            shutil.copy(resPath+OSP+'mipmap-hdpi'+OSP+'ic_launcher.png',res)

        res = outFolder+"res"+OSP+'mipmap-xxhdpi'
        if not os.path.exists(res):
            os.mkdir(res)           
        if not os.path.exists(res+OSP+"ic_launcher.png"):    
            shutil.copy(resPath+OSP+'mipmap-hdpi'+OSP+'ic_launcher.png',res)

        if os.path.exists(outFolder+OSP+'AndroidManifest.xml'):
            with open(outFolder+OSP+'AndroidManifest.xml') as data:
                match = re.compile('package="(.*?)".*?android:label="(.*?)".*?<activity android:name="(.*?)"', re.MULTILINE|re.DOTALL).findall(str(data.readlines())) 
                if match:
                    lista=match[0]
                    textEdit.PACKAGE=lista[0]
                    textEdit.ACTIVITY=lista[2]
                    textEdit.LABEL=lista[1]
                
        else:
            textEdit.ACTIVITY="android.app.NativeActivity"
            textEdit.LABEL="MyGame"
            textEdit.PACKAGE="com.djokersoft.game"
            
            
     
     
        
        win = AppAndroidSetup(self)
        textEdit.PACKAGE=sanitize_android_package_name(textEdit.PACKAGE)
        win.load(textEdit)
        
        manifFile=outFolder+"AndroidManifest.xml"
        manifest_data = build_native_manifest(
            NATIVE_MANIFEST_TEMPLATE,
            textEdit.PACKAGE,
            textEdit.LABEL,
            textEdit.ACTIVITY,
            Name,
        )
        with open(manifFile, "w") as text_file:
            text_file.write(manifest_data)
        
            
    def addSDL2Template(self):
        print("sdl2")
        pass
        
    def setupCompilerActions(self):
        tb = QToolBar(self)
        tb.setWindowTitle("Compiler Actions")
        self.addToolBar(tb)

        menu = QMenu("&Compiler", self)
        self.menuBar().addMenu(menu)

        
        

        self.comboBox = QComboBox(tb)
        self.comboBox.setGeometry(QRect(260, 130, 390, 67))
        self.comboBox.addItem("Dektop")
        self.comboBox.addItem("Android")
        self.comboBox.addItem("Emscripten")
        self.comboBox.currentIndexChanged.connect(self.switchPlataform)
        self.comboBox.setObjectName(u"comboBox")
        sizePolicy = QSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed)
        sizePolicy.setHorizontalStretch(1)
        sizePolicy.setVerticalStretch(1)
        self.comboBox.setSizePolicy(sizePolicy)
        self.comboBox.setEditable(False)
        


        actionCompile = QAction(QIcon(rsrcPath + '/build.xpm'),           "&Compile", self,  shortcut=Qt.Key_F5, priority=QAction.LowPriority, triggered=self.compile)
        tb.addAction(actionCompile)
        menu.addAction(actionCompile)

        tb.insertWidget(actionCompile,self.comboBox)

        self.actionRun = QAction(          QIcon(rsrcPath + '/Go.png'),                "&Run", self, shortcut=Qt.Key_F6,priority=QAction.LowPriority, triggered=self.compileRun)
        tb.addAction(self.actionRun)
        menu.addAction(self.actionRun)
        
        #self.actionRun.setIcon(QIcon(rsrcPath + '/Go.png'))
        self.actionRun.isRun=False
        #self.actionRun.setIcon(QIcon(rsrcPath + '/process-stop.png'))
        

        actionCompileRun = QAction(   QIcon(rsrcPath + '/buildrun.xpm'),                "&Compile&Run", self, shortcut=Qt.Key_F9,priority=QAction.LowPriority, triggered=self.compileAndRun)
        tb.addAction(actionCompileRun)
        menu.addAction(actionCompileRun)

        actionClean= QAction(QIcon(rsrcPath + '/edit-clear.png'),                "&Clean", self, priority=QAction.LowPriority,triggered=self.compileClean)
        tb.addAction(actionClean)
        menu.addAction(actionClean)

        actionSelectModule= QAction(QIcon(rsrcPath + '/pkg_add_200.png'),                "&Add Modules", self, priority=QAction.LowPriority,triggered=self.addModule)
        tb.addAction(actionSelectModule)
        menu.addAction(actionSelectModule)
        
        actionCompileOptions= QAction(QIcon(rsrcPath + '/menu_run_parameters_200.png'),                "&Options", self, priority=QAction.LowPriority,triggered=self.compileOptions)
        tb.addAction(actionCompileOptions)
        menu.addAction(actionCompileOptions)


    def showModules(self):
         WinModules = ModulesWindow(self)
         WinModules.load()
         
    
    def setupModulesActions(self):
        tb = QToolBar(self)
        tb.setWindowTitle("Module Actions")
        self.addToolBar(tb)
        

        menu = QMenu("&Modules", self)
        self.menuBar().addMenu(menu)

              


        actionShow = QAction(QIcon(rsrcPath + '/pkg_inherited_200.png'),
                "&Build Modules", self, priority=QAction.LowPriority, triggered=self.showModules)
        
        menu.addAction(actionShow)
        tb.addAction(actionShow)
        




    def newProject(self):
        fn, fp = QFileDialog.getSaveFileName(self, "Save project as...", self.LAST_DIR,
                "Project Files mk(*.mk);;All Files (*)")

        if not fn:
            return False

        if not fn.endswith(".mk"):
            fn +=".mk"

        self.listSrc.clearAll()
        self.LAST_PROJECT=fn
        self.LAST_DIR = os.path.dirname(fn)
        self.actionSaveProject.setEnabled(True)


    def loadProject(self,filename):
        try:
            self.LAST_DIR=os.path.dirname(filename)
            project = Project(self)
            project.load(filename)
            self.LAST_PROJECT=filename
            self.load(project.Src.Src[0])
            self.listSrc.clearAll()
            self.projects.append(project)
            for src in project.Src.Src:
                self.listSrc.addSrcFile(src)
            textEdit = self.codeTab.currentWidget()
            textEdit.project=project        
            textEdit.IsProject=True
            self.setCurrentFileName()
        except Exception as e:
            print('Error Load Project  , on line {}'.format(sys.exc_info()[-1].tb_lineno), type(e).__name__, e)
            
        
    def openProject(self):
        fn, _ = QFileDialog.getOpenFileName(self, "Open Project...", self.LAST_DIR,
                 "Project Files mk(*.mk);;All Files (*)")

        self.loadProject(fn)
            

    def saveProject(self):
        textEdit = self.codeTab.currentWidget()
        if  textEdit.project:
            if textEdit.project:
                for src in self.listSrc.items:
                    textEdit.project.addSrc(src)
                    print (src)
                textEdit.project.save(None)
           

    def projectSaveAs(self):
        fn, fp = QFileDialog.getSaveFileName(self, "Save project as...", self.LAST_DIR,
                "Project Files mk(*.mk);;All Files (*)")

        if not fn:
            return False

        self.LAST_DIR=os.path.dirname(fn)
        self.LAST_PROJECT=fn

        if not fn.endswith(".mk"):
            fn +=".mk"
        
        textEdit = self.codeTab.currentWidget()
        textEdit.IsProject = True
        textEdit.project = Project(self)
        textEdit.project.addSrc(textEdit.fileName)
        
        self.listSrc.loadProject(textEdit.project)
        textEdit.project.save(fn)
        self.actionSaveProject.setEnabled(True)


    
    def setupProjectActions(self):
        menu = QMenu("&Project", self)
        self.menuBar().addMenu(menu)

        self.actionNewProject = QAction(
                QIcon.fromTheme('document-new',
                        QIcon(rsrcPath + '/filenew.png')),
                "&New", self, priority=QAction.LowPriority, triggered=self.newProject)
        
        menu.addAction(self.actionNewProject)

        self.actionOpenProject = QAction(
                QIcon.fromTheme('document-open',
                        QIcon(rsrcPath + '/fileopen.png')),
                "&Open", self,
                triggered=self.openProject)
        
        menu.addAction(self.actionOpenProject)
        menu.addSeparator()

        self.actionSaveProject = QAction(
                
                        QIcon(rsrcPath + '/filesave.png'),
                "&Save", self,                 triggered=self.saveProject, enabled=False)
        
        menu.addAction(self.actionSaveProject)

        self.actionSaveProjectAs = QAction(QIcon(rsrcPath + '/menu_saveas_200.png'),"Save &As...", self,
                priority=QAction.LowPriority,                triggered=self.projectSaveAs)
        menu.addAction(self.actionSaveProjectAs)
        menu.addSeparator()
        
        actionSelectModule= QAction(QIcon(rsrcPath + '/pkg_add_200.png'),                "&Add Module", self, priority=QAction.LowPriority,triggered=self.addModuleToProject)
        menu.addAction(actionSelectModule)

        actionProjectOptions= QAction(QIcon(rsrcPath + '/menu_compiler_options_200.png'),                "&Options", self, priority=QAction.LowPriority,triggered=self.openProjectOptions)
        menu.addAction(actionProjectOptions)
 


        menu.addSeparator()
        
    def openProjectOptions(self):
        textEdit = self.codeTab.currentWidget()
        project=textEdit.project
        if project:
            win = ProjectOptionsWindow(self,self.currentPlataform)
            win.load(project)
            
            
    def setupFileActions(self):
        tb = QToolBar(self)
        tb.setWindowTitle("File Actions")
        self.addToolBar(tb)

        menu = QMenu("&File", self)
        self.menuBar().addMenu(menu)

        self.actionNew = QAction(
                
                        QIcon(rsrcPath + '/NewFile.png'),
                "&New", self, priority=QAction.LowPriority,
                shortcut=QKeySequence.New, triggered=self.fileNew)
        tb.addAction(self.actionNew)
        menu.addAction(self.actionNew)

        self.actionOpen = QAction(
                
                        QIcon(rsrcPath + '/OpenFile.png'),
                "&Open", self, shortcut=QKeySequence.Open,
                triggered=self.fileOpen)
        tb.addAction(self.actionOpen)
        menu.addAction(self.actionOpen)
        menu.addSeparator()

        self.actionSave = QAction(
                
                        QIcon(rsrcPath + '/filesave.png'),
                "&Save", self, shortcut=QKeySequence.Save,
                triggered=self.fileSave, enabled=False)
        tb.addAction(self.actionSave)
        menu.addAction(self.actionSave)

        self.actionSaveAs = QAction(  QIcon(rsrcPath + '/menu_saveas_200.png'),"Save File &As...", self,
                priority=QAction.LowPriority,enabled=False,
                shortcut=Qt.CTRL + Qt.SHIFT + Qt.Key_S,
                triggered=self.fileSaveAs)

        menu.addAction(self.actionSaveAs)
        tb.addAction(self.actionSaveAs)
        menu.addSeparator()
 


        menu.addSeparator()

        self.actionQuit = QAction("&Quit", self, shortcut=QKeySequence.Quit,
                triggered=self.close)
        menu.addAction(self.actionQuit)

    def setupEditActions(self):
        tb = QToolBar(self)
        tb.setWindowTitle("Edit Actions")
        self.addToolBar(tb)

        menu = QMenu("&Edit", self)
        self.menuBar().addMenu(menu)

        self.actionUndo = QAction(
                QIcon.fromTheme('edit-undo',
                        QIcon(rsrcPath + '/editundo.png')),
                "&Undo", self, shortcut=QKeySequence.Undo)
        tb.addAction(self.actionUndo)
        menu.addAction(self.actionUndo)

        self.actionRedo = QAction(
                QIcon.fromTheme('edit-redo',
                        QIcon(rsrcPath + '/editredo.png')),
                "&Redo", self, priority=QAction.LowPriority,
                shortcut=QKeySequence.Redo)
        tb.addAction(self.actionRedo)
        menu.addAction(self.actionRedo)
        menu.addSeparator()

        self.actionCut = QAction(
                QIcon.fromTheme('edit-cut', QIcon(rsrcPath + '/editcut.png')),
                "Cu&t", self, priority=QAction.LowPriority,
                shortcut=QKeySequence.Cut)
        tb.addAction(self.actionCut)
        menu.addAction(self.actionCut)

        self.actionCopy = QAction(
                QIcon.fromTheme('edit-copy',
                        QIcon(rsrcPath + '/editcopy.png')),
                "&Copy", self, priority=QAction.LowPriority,
                shortcut=QKeySequence.Copy)
        tb.addAction(self.actionCopy)
        menu.addAction(self.actionCopy)

        self.actionPaste = QAction(
                QIcon.fromTheme('edit-paste',
                        QIcon(rsrcPath + '/editpaste.png')),
                "&Paste", self, priority=QAction.LowPriority,
                shortcut=QKeySequence.Paste,
                enabled=(len(QApplication.clipboard().text()) != 0))
        tb.addAction(self.actionPaste)
        menu.addAction(self.actionPaste)        
    def setProgress(self,value,code):
        print("Compile ",code," ",value)
    
    def compileFile(self,FileName,plataform):
        print("Compile ",FileName," ",plataform)
        textEdit = self.codeTab.currentWidget()
        if not textEdit:
            return
        if plataform==0:
            osName = "Linux"
            if sys.platform.startswith('windows'):
                osName = "Windows"
            elif sys.platform.startswith('linux'):
                osName = "Linux"

        
        
        
            Name=os.path.splitext(os.path.basename(FileName))[0]
            Dir=os.path.dirname(os.path.abspath(FileName))
            filename, file_extension = os.path.splitext(FileName)
            useCPP=False
            if len(file_extension)>=3:
                useCPP=True
            
            code=[]
            code.append(FileName)
        if plataform==0:
            print("Compile ",osName)
            print(FileName)
            print(Dir)
            print(Name)
            
            ARGCPP=[]
            ARGCC=[]
            ARGLD=[]
            
            for arg in self.ARGSDESKTOP.ARGCC:
                ARGCC.append(arg)
                
            for arg in self.ARGSDESKTOP.ARGCPP:
                ARGCPP.append(arg)
            
            for arg in self.ARGSDESKTOP.ARGLD:
                ARGLD.append(arg)
                
                
 
            
            useIncludes=[]
            useLibsPath=[]
                
            for name in self.GLOBALMODULES:
                module= self.getModule(name)
                if module:
                    for depend in module.Depends:
                        mdp = self.getModule(depend)
                        if mdp:
                            include = mdp.Dir+OSP+"include"
                            useIncludes.append("-I"+include)
                            lib = mdp.Name
                            libPath = mdp.Dir+OSP+osName
                            pathLib=  libPath+OSP+'lib'+lib+'.a'
                            for inc in mdp.MainSrc.Include:
                                useIncludes.append("-I"+mdp.Dir+OSP+inc)
  
                            for inc in mdp.DesktopSrc.Include:
                                useIncludes.append("-I"+mdp.Dir+OSP+inc)
                            useLibsPath.append("-L"+libPath)
                   
                        
                    
                    include = module.Dir+OSP+"include"
                    useIncludes.append("-I"+include)
                    include = module.Dir+OSP+"include"+OSP+"linux"
                    useIncludes.append("-I"+include)
                    
                    
                    libPath = module.Dir+OSP+osName
                    useLibsPath.append("-L"+libPath)
                    for inc in module.MainSrc.Include:
                        useIncludes.append("-I"+module.Dir+OSP+inc)

                    
                    for inc in module.DesktopSrc.Include:
                        useIncludes.append("-I"+module.Dir+OSP+inc)

                        
                    lib = module.Name
                    pathLib=  libPath+OSP+'lib'+lib+'.a'
                    if module.Static==False:
                        pathLib=  libPath+OSP+'lib'+lib+'.so'
                        
                    for inc in useIncludes:
                        ARGCC.append(inc)
                        ARGCPP.append(inc)
                    for libpath in useLibsPath:
                        ARGLD.append(libpath)  
                                         
                    
                    if os.path.exists(pathLib):
                        ARGLD.append("-l"+lib)
                    
                    for arg in module.MainSrc.ARGS.ARGLD:
                        ARGLD.append(arg)
                      
                    for arg in module.DesktopSrc.ARGS.ARGLD:
                        ARGLD.append(arg)
                    

                
            
            
            if DesktopCompile(self,Dir,Name,code,ARGCC,ARGCPP,True):
                DesktopBuild(self,Dir,Name,useCPP,ARGLD,0)
                
        elif plataform==1:
            osName = "Android"
            Name=os.path.splitext(os.path.basename(FileName))[0]
            Dir=os.path.dirname(os.path.abspath(FileName))
            filename, file_extension = os.path.splitext(FileName)
            useCPP=False
            if len(file_extension)>=3:
                useCPP=True
            
            code=[]
            code.append(FileName)
            
            print("Compile ",osName)
            print(FileName)
            print(Dir)
            print(Name)
            
            ARGCPP=[]
            ARGCC=[]
            ARGLD=[]
            
            for arg in self.ARGSANDROID.ARGCC:
                ARGCC.append(arg)

            for arg in self.ARGSANDROID.ARGCPP:
                ARGCPP.append(arg)
            
            for arg in self.ARGSANDROID.ARGLD:
                ARGLD.append(arg)
                
                
        
                 
            useIncludes=[]
            useLibsPath=[]    
            
            for name in self.GLOBALMODULES:
                module= self.getModule(name)
                if module:
                    for depend in module.Depends:
                        mdp = self.getModule(depend)
                        if mdp:
                            include = mdp.Dir+OSP+"include"
                            useIncludes.append("-I"+include)
                            lib = mdp.Name
                            libPath = mdp.Dir+OSP+osName+OSP+"armeabi-v7a"
                            for inc in mdp.MainSrc.Include:
                                useIncludes.append("-I"+mdp.Dir+OSP+inc)
                            for inc in mdp.AndroidSrc.Include:
                                useIncludes.append("-I"+mdp.Dir+OSP+inc)
                            useLibsPath.append("-L"+libPath)
                        
                    
                    include = module.Dir+OSP+"include"
                    useIncludes.append("-I"+include)
                    
                    include = module.Dir+OSP+"include"+OSP+"android"
                    useIncludes.append("-I"+include)
                    
                    libPath = module.Dir+OSP+osName+OSP+"armeabi-v7a"
                    useLibsPath.append("-L"+libPath)
      
                    
                    
                    for inc in module.MainSrc.Include:
                        useIncludes.append("-I"+module.Dir+OSP+inc)
                    
                    for inc in module.AndroidSrc.Include:
                        useIncludes.append("-I"+module.Dir+OSP+inc)

                    
                    lib = module.Name
                    
                    pathLib=  libPath+OSP+'lib'+lib+'.a'
                    if module.Static==False:
                        pathLib=  libPath+OSP+'lib'+lib+'.so'

                    for inc in useIncludes:
                        ARGCC.append(inc)
                        ARGCPP.append(inc)
                        
                    for libpath in useLibsPath:
                        ARGLD.append(libpath)  
                                         
                    
                    if os.path.exists(pathLib):
                        ARGLD.append("-l"+lib)
                    
                    
                    for arg in module.MainSrc.ARGS.ARGLD:
                        ARGLD.append(arg)
                      
                    for arg in module.AndroidSrc.ARGS.ARGLD:
                        ARGLD.append(arg)

    
            outFolder=Dir+OSP+"Android"+OSP+Name+OSP
            
            if os.path.exists(outFolder+'AndroidManifest.xml'):
                with open(outFolder+'AndroidManifest.xml') as data:
                    match = re.compile('package="(.*?)".*?android:label="(.*?)".*?<activity android:name="(.*?)"', re.MULTILINE|re.DOTALL).findall(str(data.readlines())) 
                    if match:
                        lista=match[0]
                        textEdit.PACKAGE=sanitize_android_package_name(lista[0])
                        textEdit.ACTIVITY=lista[2]
                        textEdit.LABEL=lista[1]
            if not textEdit.PACKAGE:
                textEdit.PACKAGE="com.djokersoft.game"
            if not textEdit.ACTIVITY:
                textEdit.ACTIVITY="android.app.NativeActivity"
            if not textEdit.LABEL:
                textEdit.LABEL=Name

            ANDROID_PACK    =sanitize_android_package_name(textEdit.PACKAGE)
            textEdit.PACKAGE=ANDROID_PACK
            ANDROID_LABEL   =textEdit.LABEL
            ANDROID_ACTIVITY=textEdit.ACTIVITY
            ANDROID_PACK, ANDROID_LABEL, ANDROID_ACTIVITY = ensure_android_native_template(
                self,
                Dir,
                Name,
                ANDROID_PACK,
                ANDROID_LABEL,
                ANDROID_ACTIVITY,
            )
                
            if AndroidCompile(self,Dir,Name,code,ARGCC,ARGCPP,0,True):
                if AndroidBuild(self,Dir,Name,useCPP,ARGLD,0,0):
                    androidCompileJavaNative(self,Dir,0,Name,ANDROID_PACK,ANDROID_LABEL,ANDROID_ACTIVITY,False)
                    
                    
                
            else:
                self.trace("Fail compile")
                
            pass
        elif plataform==2:
            
            osName = "Web"
            Name=os.path.splitext(os.path.basename(FileName))[0]
            Dir=os.path.dirname(os.path.abspath(FileName))
            filename, file_extension = os.path.splitext(FileName)
            useCPP=False
            if len(file_extension)>=3:
                useCPP=True
            
            code=[]
            code.append(FileName)
            
            print("Compile ",osName)
            print(FileName)
            print(Dir)
            print(Name)
            
            ARGCPP=[]
            ARGCC=[]
            ARGLD=[]
            
            for arg in self.ARGSWEB.ARGCC:
                ARGCC.append(arg)
                
                
            for arg in self.ARGSWEB.ARGCPP:
                ARGCPP.append(arg)
            
            for arg in self.ARGSWEB.ARGLD:
                ARGLD.append(arg)
                
                
                             
            useIncludes=[]
            useLibsPath=[]    
            
            Template=None    
            for name in self.GLOBALMODULES:
                module= self.getModule(name)
                if module:
                    for depend in module.Depends:
                        mdp = self.getModule(depend)
                        if mdp:
                            include = mdp.Dir+OSP+"include"
                            useIncludes.append("-I"+include)
                            if mdp.WebSrc.SHELL:
                                if os.path.exists(mdp.Dir+OSP+mdp.WebSrc.SHELL):
                                    Template=mdp.Dir+OSP+mdp.WebSrc.SHELL
                                    
             
                            libPath = mdp.Dir+OSP+osName
                            for inc in mdp.MainSrc.Include:
                                useIncludes.append("-I"+mdp.Dir+OSP+inc)
                            
                            for inc in mdp.WebSrc.Include:
                                useIncludes.append("-I"+mdp.Dir+OSP+inc)
                                
                            lib = mdp.Name
                            pathLib=  libPath+OSP+'lib'+lib+'.a'
                            useLibsPath.append("-L"+libPath)
                            
                    
                    
                    

                    
                    
                    if module.WebSrc.SHELL:
                        if os.path.exists(module.Dir+OSP+module.WebSrc.SHELL):
                            Template=module.Dir+OSP+module.WebSrc.SHELL
                    
                    lib = module.Name
                    include = module.Dir+OSP+"include"
                    useIncludes.append("-I"+include)
                    include = module.Dir+OSP+"include"+OSP+"web"
                    useIncludes.append("-I"+include)
                    libPath = module.Dir+OSP+osName
                    useLibsPath.append("-L"+libPath)
                    
                    for inc in module.MainSrc.Include:
                        useIncludes.append("-I"+module.Dir+OSP+inc)
                    
                    for inc in module.WebSrc.Include:
                        useIncludes.append("-I"+module.Dir+OSP+inc)

                    
                    
                    for inc in useIncludes:
                        ARGCC.append(inc)
                        ARGCPP.append(inc)
                    
                    lib = module.Name
                    pathLib=  libPath+OSP+'lib'+lib+'.a'
                    
     
                    
                    ARGLD.append("-L"+libPath)
                    for libpath in useLibsPath:
                        ARGLD.append(libpath)  
                                         
                    
                      
                    if os.path.exists(pathLib):
                        ARGLD.append("-l"+lib)
                        
                    

                    for arg in module.MainSrc.ARGS.ARGLD:
                        ARGLD.append(arg)                        
                    
                    for arg in module.WebSrc.ARGS.ARGLD:
                        ARGLD.append(arg)
           
                    if Template:
                        self.trace("Use shell",Template)
                        ARGLD.append('--shell-file')
                        ARGLD.append(Template)
                        #template:=' --shell-file '+modsPaht+module.name+PS+'shell.html ';
                        #         resources:='--preload-file '+filepath+'resources@resources';
                        
                    if os.path.exists(Dir+OSP+"media"):
                        print("*** add",Dir+OSP+"media")
                        ARGLD.append('--preload-file')
                        ARGLD.append(Dir+OSP+"media"+"@media")
                        
                    elif os.path.exists(Dir+OSP+"resources"):
                        print("*** add",Dir+OSP+"resources")
                        ARGLD.append('--preload-file')
                        ARGLD.append(Dir+OSP+"resources"+"@resources")
                    elif os.path.exists(Dir+OSP+"data"):
                        print("*** add",Dir+OSP+"data")
                        ARGLD.append('--preload-file')
                        ARGLD.append(Dir+OSP+"data"+"@data")
                    elif os.path.exists(Dir+OSP+"media"):
                        print("*** add",Dir+OSP+"media")
                        ARGLD.append('--preload-file')
                        ARGLD.append(Dir+OSP+"media"+"@media")
                        
                   
                    
                
                
            if WebCompile(self,Dir,Name,code,ARGCC,ARGCPP,False):
                if not WebBuild(self,Dir,Name,useCPP,ARGLD,False):
                    self.trace("Web build/link failed for ", Name)
            else:
                self.trace("Web compile failed for ", Name)
                
                
                
    
    def execute(self,cmd,args):
        args = [cmd] + args
        re = subprocess.Popen(args, shell=False,close_fds=True,stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        
        chkdata = re.stdout.readlines()
        for line in chkdata:
            lstr=line.decode('utf-8')
            self.trace(lstr)
            
    def runFile(self,FileName,plataform):
        textEdit = self.codeTab.currentWidget()
        project = textEdit.project
        Name=os.path.splitext(os.path.basename(FileName))[0]
        Dir=os.path.dirname(os.path.abspath(FileName))
        filename, file_extension = os.path.splitext(FileName)
        
        
        if plataform==0:
            print("Run linux")
            print(filename)
            print(Dir)
            print(Name)
            if os.path.exists(filename):
                try:

                        if 'LD_LIBRARY_PATH' not in os.environ:
                            os.environ['LD_LIBRARY_PATH'] = '/media/djoker/code/linux/python/projects/CppEditor/modules/ogre/Linux'
        
                        self.worker = ExecuteWorker(self,filename,self.currentPlataform)
                        self.worker.traceMessage.connect(self.log)
                        self.worker.start()
                        
                        #xterm -T ogresdl -e /usr/bin/cb_console_runner LD_LIBRARY_PATH=:.:/media/djoker/code/local/user/lib /media/djoker/code/linux/cpp/ogresdl/bin/Debug/ogresdl  (in /media/djoker/code/linux/cpp/ogresdl/.)
                        
                    
                                     
                except Exception as e:
                    self.trace('Error on line {}'.format(sys.exc_info()[-1].tb_lineno), type(e).__name__, e)

                #process = subprocess.Popen(
                #shlex.split("""x-terminal-emulator -e 'bash -c "/media/djoker/code/linux/python/guis/tabs.py"'"""), stdout=subprocess.PIPE)
                #shlex.split("""x-terminal-emulator -e '/bin/sh "/media/djoker/code/linux/python/projects/CppEditor/projects/main"'"""), stdout=subprocess.PIPE)
                #process.wait()
                #print (process.returncode)
        elif plataform==2:
            print("Run Web")
            Name=os.path.splitext(os.path.basename(FileName))[0]
            Dir=os.path.dirname(os.path.abspath(FileName))
            filename, file_extension = os.path.splitext(FileName)
            export = resolveWebExport(Dir, Name)
            if export:
                self.trace(export)
            
            if not export:
                self.trace("Web output not found for ", Name, " (trying build)")
                self.compileFile(FileName,2)
                export = resolveWebExport(Dir, Name)
                if export:
                    self.trace(export)

            if export and os.path.exists(export):
                try:
                    
                    self.runner = ExecuteWorker(self,export,self.currentPlataform)
                    self.runner.traceMessage.connect(self.log)
                    self.runner.start()
                
                    #runProcess(os.getcwd()+OSP+"cserver_linux",[export],False)
        
                                     
                except Exception as e:
                    self.trace('Error execute {}'.format(sys.exc_info()[-1].tb_lineno), type(e).__name__, e)
            else:
                self.trace("Web output not found for ", Name)
                self.trace("Expected: ", Dir+os.path.sep+"Web"+os.path.sep+Name+".html")
                self.trace("or: ", Dir+os.path.sep+"Web"+os.path.sep+Name+os.path.sep+Name+".html")
            
            
                 
            
            #cmd(currPaht+'cserver_linux  '+filepath+extractfilename(appname)+'.html',false);
                
            
            
        
        
            
        
    def _startProjectCompile(self, project, run_after=False):
        if self.projectCompiler and self.projectCompiler.isRunning():
            self.trace("Project compile is already running")
            return False

        self.projectCompiler = ProjectCompileWorker(self, project, self.currentPlataform, run_after)
        self.projectCompiler.traceMessage.connect(self.log)
        self.projectCompiler.compileFinished.connect(self.onProjectCompileFinished)
        self.projectCompiler.start()
        return True

    def onProjectCompileFinished(self, success, plataform, run_after, project):
        if not success:
            self.trace("Project compile failed")

        if success and run_after and project:
            project.run(plataform)

        if self.projectCompiler:
            self.projectCompiler.deleteLater()
            self.projectCompiler = None

    def compile(self):
        self.ClearConsole()
        self.fileSave()
        textEdit = self.codeTab.currentWidget()
        if not textEdit:
            return

        project = textEdit.project
        if project:
            self._startProjectCompile(project, False)
        else:
            self.compileFile(textEdit.fileName,self.currentPlataform)

    def compileRun(self):
        self.ClearConsole()
        textEdit = self.codeTab.currentWidget()
        if self.runner:
            print("stop execution")
            self.runner.stop()
            self.runner=None
            return 
            
        
            
        project = textEdit.project
        if project:
            project.run(self.currentPlataform)
        else:
            self.runFile(textEdit.fileName,self.currentPlataform)

    def compileAndRun(self):
        self.ClearConsole()
        self.fileSave()
        textEdit = self.codeTab.currentWidget()
        if not textEdit:
            return

        project = textEdit.project
        if project:
            self._startProjectCompile(project, True)
        else:
            self.compile()
            self.compileRun()
    
    def onAbort(self):
        print("Abort")
        if self.runner:
            self.runner.stop()
            self.runner=None
            
        self.actionRun.isRun=False

    def compileClean(self):
        textEdit = self.codeTab.currentWidget()
        project = textEdit.project

        if project:
            project.clean(self.currentPlataform)
            
                
    def load(self, f):
        print("load :",f)
        if not f in self.files:
            self.files.append(f)
        else:
            return False
        
        if not QFile.exists(f):
            return False

        fh = QFile(f)
        if not fh.open(QFile.ReadOnly):
            return False

        data = fh.readAll()
        
        codec = QTextCodec.codecForHtml(data)
        unistr = codec.toUnicode(data)
        self.createEditorFromFile(f,unistr)
        


        return True

    def maybeSave(self):
    
        if self.fileName.startswith(':/'):
            return True

        ret = QMessageBox.warning(self, "Application",
                "The document has been modified.\n"
                "Do you want to save your changes?",
                QMessageBox.Save | QMessageBox.Discard | QMessageBox.Cancel)

        if ret == QMessageBox.Save:
            return self.fileSave()

        if ret == QMessageBox.Cancel:
            return False

        return True

    def setCurrentFileName(self):
        textEdit = self.codeTab.currentWidget()
        index= self.codeTab.currentIndex()
        if textEdit:
            if textEdit.IsProject:
                self.codeTab.setTabText(index,os.path.basename(textEdit.project.Name))
                self.setWindowTitle(self.tr("%s[*] - %s" % (os.path.basename(textEdit.project.Name), "Cross IDE Editor @DjokerSoft.")))
            else:
                self.codeTab.setTabText(index,os.path.basename(textEdit.fileName))
                self.setWindowTitle(self.tr("%s[*] - %s" % (os.path.basename(textEdit.fileName), "Cross IDE Editor @DjokerSoft.")))
                
        self.setWindowModified(False)
    
    def setCursor(self,x,y):
        textEdit = self.codeTab.currentWidget()
        if textEdit:
            textEdit.setCursorPosition(x,y)

    def fileNew(self):
        self.createEditor()
        
        
    def fileOpen(self):
        fn, _ = QFileDialog.getOpenFileName(self, "Open File...", self.LAST_DIR,
                "Code files (*.cpp *.c);;All Files (*)")

        self.LAST_DIR=os.path.dirname(fn)
        if fn:
            self.load(fn)

    def fileSave(self):


        index =self.codeTab.currentIndex() 
        textEdit = self.codeTab.currentWidget() 
        textEdit.setModified(False)
        self.setCurrentFileName()


        if not textEdit.fileName:
            return self.fileSaveAs()
        
        
        try:
            with open(textEdit.fileName, 'w') as file:
                file.write(textEdit.text())
        except  Exception as error:
            print(" Error :",error)
            return False
        return True
        

    def fileSaveAs(self):
        textEdit = self.codeTab.currentWidget() 
        fn, _ = QFileDialog.getSaveFileName(self, "Save as...", self.LAST_DIR, "Code Files (*.cpp *.c);;Header files (*.h);;All Files (*)")
        if not fn:
            return False
        

        lfn = fn.lower()
        if not lfn.endswith(('.cpp', '.h', '.c')):
            fn += '.c'
        
        textEdit.fileName=fn
        self.LAST_DIR=os.path.dirname(fn)

        
        

        
        
        #print(textEdit)
        #print(fn)
        #print(self.codeTab.currentIndex())
        #print(self.codeTab.count())
        #print(textEdit.text())
        #self.setCurrentFileName(fn)
        return self.fileSave()

 


    def clipboardDataChanged(self):
        self.actionPaste.setEnabled(len(QApplication.clipboard().text()) != 0)

    def about(self):
        QMessageBox.about(self, "About", 
                "Cross Plataform compiler "
                "Djokersoft "
                "Luis Santos @2021")



 
    def createEditorFromFile(self,fileName,data):
        textEdit = CodeEditor(self.listSrc,self)
        textEdit.setLexer(self.cpplexer)
        textEdit.setFocus()
        self.actionSaveAs.setEnabled(True)
        self.actionSave.setEnabled(True)
        textEdit.fileName=fileName
        
        #textEdit.project.Name=os.path.splitext(os.path.basename(fileName))[0]
        
        #textEdit.isProject=False
        #textEdit.project.addSrc(fileName)
        
        textEdit.setText(data)
        self.actionUndo.triggered.connect(textEdit.undo)
        self.actionRedo.triggered.connect(textEdit.redo)
        self.actionCut.setEnabled(False)
        self.actionCopy.setEnabled(False)
        self.actionCut.triggered.connect(textEdit.cut)
        self.actionCopy.triggered.connect(textEdit.copy)
        self.actionPaste.triggered.connect(textEdit.paste)
        textEdit.copyAvailable.connect(self.actionCut.setEnabled)
        textEdit.copyAvailable.connect(self.actionCopy.setEnabled)
        tabIndex = self.codeTab.addTab(textEdit, os.path.basename(fileName))
        self.codeTab.setCurrentIndex(tabIndex)
        textEdit.modificationChanged.connect(self.actionSave.setEnabled)  
        self.setCurrentFileName()
        self.setCursor(1,1)
        

    def createEditor(self):
        textEdit = CodeEditor(self.listSrc,self)
        self.actionSaveAs.setEnabled(True)
        textEdit.setLexer(self.cpplexer)
        textEdit.setFocus()
        textEdit.IsProject = False
        textEdit.fileName="untitled.c"
        textEdit.setText("""\
int main(void) 
{
    return 0;
}\
                        """)
        self.actionUndo.triggered.connect(textEdit.undo)
        self.actionRedo.triggered.connect(textEdit.redo)
        self.actionCut.setEnabled(False)
        self.actionCopy.setEnabled(False)
        self.actionCut.triggered.connect(textEdit.cut)
        self.actionCopy.triggered.connect(textEdit.copy)
        self.actionPaste.triggered.connect(textEdit.paste)
        textEdit.copyAvailable.connect(self.actionCut.setEnabled)
        textEdit.copyAvailable.connect(self.actionCopy.setEnabled)
        tabIndex = self.codeTab.addTab(textEdit, os.path.basename(textEdit.fileName))
        self.codeTab.setCurrentIndex(tabIndex)
        textEdit.modificationChanged.connect(self.actionSave.setEnabled)
        self.setCurrentFileName()
        self.setCursor(1,1)
        
    def textChnaged(self):
        print("text change")        


    


    def printFile(self):
        editor = self.codeTab.currentWidget()
        print(editor.Text())

    def loadModules(self,realod=False):
        if realod==True:
            print("Realod")
            #self.modulesData=[]
            #self.modulesList=[]
            self.modulesData.clear()
            self.modulesList.clear()
        else:
            if len(self.modulesData)>=1:
                print("modules is loaded")
                return
           
        dirs = getFoldersTree(modulesPath) 
        for dir in dirs:
            #print(os.path.join(root, name)," ",ext," ",directories)
            module=dir + OSP +"module.json"
            self.modulesList.append(module)
            print("Load :",module)
            
            with open(module) as data:
                try:
                    jdata = json.load(data)
                    cModule=CppModule(self)
                    cModule.Dir=dir
                    cModule.Name = jdata["module"]
                    cModule.About = jdata["about"]
                    cModule.Author = jdata["author"]
                    cModule.Version = jdata["version"]
                    cModule.Static = jdata["static"]
                    cModule.Priority=0# = jdata["priority"]
                    

                    #self.trace("Load ",cModule.Name," module .")
                    #self.trace("About ",cModule.About)
                    #self.trace("Author ",cModule.Author)
                    #self.trace("Version ",cModule.Version)

                    depends = jdata["depends"]
                    for depend in depends:
                        cModule.Depends.append(depend)

                    system = jdata["system"]
                    if "linux" in system:
                        cModule.useLinux=True
                    elif "windows" in system:
                        cModule.useWindows=True
                    elif "emscripten" in system:
                        cModule.useWeb=True
                    elif "android" in system:
                        cModule.useAndroid=True
                        
                    
                    
                    for sys in system:
                        cModule.System.append(sys)
                        
                        
                    cModule.MainWindow=self
                    
                    for src in jdata["src"]:
                        data = src.strip()
                        if len(data)<=1:
                            continue
                        cModule.MainSrc.Src.append(data)
                        if showSRCLoad:
                            print("src: ",data)

                    for src in jdata["include"]:
                        if len(src.strip())>1:
                            cModule.MainSrc.Include.append(src.strip())
                        
                    
                    plataform = jdata["plataforms"]
                    if not plataform:
                        continue

                    if (isLinux):
                        linux = plataform ["linux"]
                        for src in linux["src"]:
                            data = src.strip()
                            if len(data)<=1:
                                continue
                            cModule.DesktopSrc.Src.append(data)

                            if showSRCLoad:
                                print("linux ",data)       

                            
                        for src in linux["include"]:
                            if len(src.strip())>1:
                                cModule.DesktopSrc.Include.append(src.strip())
                        
                        cppargs = linux["CPP_ARGS"].split(" ")
                        if len(cppargs)>=1:
                            for arg in cppargs:
                                value =arg.strip()
                                if len(value)>=1:
                                    cModule.DesktopSrc.ARGS.addCPP(value)

                        ccargs = linux["CC_ARGS"].split(" ")
                        if len(ccargs)>=1:
                            for arg in ccargs:
                                value =arg.strip()
                                if len(value)>=1:
                                    cModule.DesktopSrc.ARGS.addCC(value)


                        ldargs = linux["LD_ARGS"].split(" ")
                        if len(ldargs)>=1:
                            for arg in ldargs:
                                value =arg.strip()
                                if len(value)>=1:
                                    cModule.DesktopSrc.ARGS.addLD(value)            
                    else:
                        windows = plataform ["windows"]

                        for src in windows["src"]:
                            data = src.strip()
                            if len(data)<=1:
                                continue
                            cModule.DesktopSrc.Src.append(data)

                            if showSRCLoad:
                                print("window ",src)                            
                            
                        for src in windows["include"]:
                            if len(src.strip())>1:
                                cModule.DesktopSrc.Include.append(src.strip())
                                
                        cppargs = windows["CPP_ARGS"].split(" ")
                        if len(cppargs)>=1:
                            for arg in cppargs:
                                value =arg.strip()
                                if len(value)>=1:
                                    cModule.DesktopSrc.ARGS.addCPP(value)

                        ccargs = windows["CC_ARGS"].split(" ")
                        if len(ccargs)>=1:
                            for arg in ccargs:
                                value =arg.strip()
                                if len(value)>=1:
                                    cModule.DesktopSrc.ARGS.addCC(value)


                        ldargs = windows["LD_ARGS"].split(" ")
                        if len(ldargs)>=1:
                            for arg in ldargs:
                                value =arg.strip()
                                if len(value)>=1:
                                    cModule.DesktopSrc.ARGS.addLD(value)  
                    
                    android = plataform["android"]
                    for src in android["src"]:
                        data = src.strip()
                        if len(data)<=1:
                            continue
                        cModule.AndroidSrc.Src.append(data)
      

                        if showSRCLoad:
                            print("android ",src)                            

                        
                    for src in android["include"]:
                        if len(src.strip())>1:
                            cModule.AndroidSrc.Include.append(src.strip())

                    cppargs = android["CPP_ARGS"].split(" ")
                    if len(cppargs)>=1:
                        for arg in cppargs:
                            value =arg.strip()
                            if len(value)>=1:
                                cModule.AndroidSrc.ARGS.addCPP(value)

                    ccargs = android["CC_ARGS"].split(" ")
                    if len(ccargs)>=1:
                        for arg in ccargs:
                            value =arg.strip()
                            if len(value)>=1:
                                cModule.AndroidSrc.ARGS.addCC(value)


                    ldargs = android["LD_ARGS"].split(" ")
                    if len(ldargs)>=1:
                        for arg in ldargs:
                            value =arg.strip()
                            if len(value)>=1:
                                cModule.AndroidSrc.ARGS.addLD(value)  


                    emscripten = plataform ["emscripten"]
                    cModule.WebSrc.SHELL=emscripten["template"]
                    for src in emscripten["src"]:
                        data = src.strip()
                        if len(data)<=1:
                            continue
                        cModule.WebSrc.Src.append(data)
                               
                        if showSRCLoad:
                            print("emscripten ",src)                            

                        
                    for src in emscripten["include"]:
                        if len(src.strip())>1:
                            cModule.WebSrc.Include.append(src.strip())     

                    cppargs = android["CPP_ARGS"].split(" ")
                    if len(cppargs)>=1:
                        for arg in cppargs:
                            value =arg.strip()
                            if len(value)>=1:
                                cModule.WebSrc.ARGS.addCPP(value)

                    ccargs = emscripten["CC_ARGS"].split(" ")
                    if len(ccargs)>=1:
                        for arg in ccargs:
                            value =arg.strip()
                            if len(value)>=1:
                                cModule.WebSrc.ARGS.addCC(value)


                    ldargs = emscripten["LD_ARGS"].split(" ")
                    if len(ldargs)>=1:
                        for arg in ldargs:
                            value =arg.strip()
                            if len(value)>=1:
                                #cModule.WebSrc.ARGS.addLD(value)
                                cModule.WebSrc.ARGS.ARGLD.append(value)
                    self.modulesData.append(cModule)  
         
                except Exception as e:
                    print('Error Load Module  , on line {}'.format(sys.exc_info()[-1].tb_lineno), type(e).__name__, e)
    
    def getModules(self):
        return self.modulesData

    def getModule(self,name):
        for module in self.modulesData:
            if module.Name==name:
                return module
    def closeEvent(self, event):
        
        projList=[]
        for project in self.projects:
            projList.append(project.Root)
        
        #print(modulesList)
        
        with open("config.json", 'w') as f:
                f.write('{ \n')
                
                f.write('  "Configuration":{ \n')
                
                f.write('  "Modules":'+str(self.GLOBALMODULES).replace("'",'"')+',\n')
                f.write('  "Projects":'+str(projList).replace("'",'"')+',\n')
                f.write('  "Files":'+str(self.files).replace("'",'"')+',\n')
                f.write('\n')
                f.write('  "Desktop":{ \n')
                f.write('  "ArgCPP":'+str(self.ARGSDESKTOP.ARGCPP).replace("'",'"')+',\n')
                f.write('  "ArgCC":'+str(self.ARGSDESKTOP.ARGCC).replace("'",'"')+',\n')
                f.write('  "ArgLD":'+str(self.ARGSDESKTOP.ARGLD).replace("'",'"')+'\n')
                f.write('  },\n')
                f.write('\n')
                f.write('  "Android":{ \n')
                f.write('  "ArgCPP":'+str(self.ARGSANDROID.ARGCPP).replace("'",'"')+',\n')
                f.write('  "ArgCC":'+str(self.ARGSANDROID.ARGCC).replace("'",'"')+',\n')
                f.write('  "ArgLD":'+str(self.ARGSANDROID.ARGLD).replace("'",'"')+'\n')
                f.write('  },\n')
                f.write('\n')
                f.write('  "Web":{ \n')
                f.write('  "ArgCPP":'+str(self.ARGSWEB.ARGCPP).replace("'",'"')+',\n')
                f.write('  "ArgCC":'+str(self.ARGSWEB.ARGCC).replace("'",'"')+',\n')
                f.write('  "ArgLD":'+str(self.ARGSWEB.ARGLD).replace("'",'"')+'\n')
                f.write('  }\n')
                
                                

                f.write('            }\n')
                f.write('} \n')
                

        
        
        event.accept()
        '''
        close = QMessageBox()
        close.setText("You sure?")
        close.setStandardButtons(QMessageBox.Yes | QMessageBox.Cancel)
        close = close.exec()

        if close == QMessageBox.Yes:
            event.accept()
        else:
            event.ignore()
        '''



#def verify(self):
# if self.nameEdit.text() and self.addressEdit.toPlainText():
#self.accept()
 #   return

#answer = QMessageBox.warning(self, "Incomplete Form",
#        "The form does not contain all the necessary information.\n"
#       "Do you want to discard it?",
#                QMessageBox.Yes, QMessageBox.No)

#if answer == QMessageBox.Yes:
#    self.reject()


if __name__ == '__main__':

    import sys
    if (sys.platform=="linux"):
        isLinux=True
        print(sys.platform)
    else:
        isLinux=False
        print(sys.platform)


    '''proj = Project()
    proj.load("projects/main.mk")
    proj = Project()
    proj.Dir="projects/main"
    proj.Modules.append("raylib")
    proj.Modules.append("box2d")
    proj.Modules.append("jpg")

    proj.addSrc("main/main.c")
    proj.addSrc("main/core.c")
    proj.addSrc("main/utils.c")

    proj.save("main.mk")'''


    if 'LD_LIBRARY_PATH' not in os.environ:
        os.environ['LD_LIBRARY_PATH'] = modulesPath + 'ogre/Linux'
        
        
        
        
        
    

    
    
            
    app = QApplication(sys.argv)
    window = MainWindow()
    window.resize(1024, 780)
    centerPoint = QDesktopWidget().availableGeometry().center()
    
    qtRectangle = window.frameGeometry()
    qtRectangle.moveCenter(centerPoint)
    
    window.move(qtRectangle.topLeft())
    
    window.show()
    
    print ('Success:', os.environ['LD_LIBRARY_PATH'])
    #window.createSample()
    sys.exit(app.exec_())



'''
pip3 install qscintilla
pip3 install PyQtWebEngine

'''
