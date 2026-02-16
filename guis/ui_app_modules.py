# -*- coding: utf-8 -*-

################################################################################
## Form generated from reading UI file 'app_modulesVuEZsE.ui'
##
## Created by: Qt User Interface Compiler version 5.15.2
##
## WARNING! All changes made in this file will be lost when recompiling UI file!
################################################################################

from PySide2.QtCore import *
from PySide2.QtGui import *
from PySide2.QtWidgets import *


class Ui_Dialog(object):
    def setupUi(self, Dialog):
        if not Dialog.objectName():
            Dialog.setObjectName(u"Dialog")
        Dialog.resize(965, 314)
        sizePolicy = QSizePolicy(QSizePolicy.Maximum, QSizePolicy.Minimum)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(Dialog.sizePolicy().hasHeightForWidth())
        Dialog.setSizePolicy(sizePolicy)
        Dialog.setWindowTitle(u"Dialog")
        self.gridLayout_3 = QGridLayout(Dialog)
        self.gridLayout_3.setObjectName(u"gridLayout_3")
        self.groupBox = QGroupBox(Dialog)
        self.groupBox.setObjectName(u"groupBox")
        sizePolicy1 = QSizePolicy(QSizePolicy.Expanding, QSizePolicy.Minimum)
        sizePolicy1.setHorizontalStretch(0)
        sizePolicy1.setVerticalStretch(0)
        sizePolicy1.setHeightForWidth(self.groupBox.sizePolicy().hasHeightForWidth())
        self.groupBox.setSizePolicy(sizePolicy1)
        self.groupBox.setTitle(u"Modules")
        self.gridLayout = QGridLayout(self.groupBox)
        self.gridLayout.setObjectName(u"gridLayout")
        self.apagar = QCheckBox(self.groupBox)
        self.apagar.setObjectName(u"apagar")
        self.apagar.setText(u"CheckBox")

        self.gridLayout.addWidget(self.apagar, 0, 0, 1, 1)


        self.gridLayout_3.addWidget(self.groupBox, 0, 0, 1, 3)

        self.verticalSpacer = QSpacerItem(20, 2, QSizePolicy.Minimum, QSizePolicy.MinimumExpanding)

        self.gridLayout_3.addItem(self.verticalSpacer, 1, 1, 1, 1)

        self.groupBoxBuildOptions = QGroupBox(Dialog)
        self.groupBoxBuildOptions.setObjectName(u"groupBoxBuildOptions")
        sizePolicy1.setHeightForWidth(self.groupBoxBuildOptions.sizePolicy().hasHeightForWidth())
        self.groupBoxBuildOptions.setSizePolicy(sizePolicy1)
        self.groupBoxBuildOptions.setTitle(u"Compile & Build ARGS Options")
        self.gridLayout_2 = QGridLayout(self.groupBoxBuildOptions)
        self.gridLayout_2.setObjectName(u"gridLayout_2")
        self.label = QLabel(self.groupBoxBuildOptions)
        self.label.setObjectName(u"label")
        self.label.setText(u"CPP")

        self.gridLayout_2.addWidget(self.label, 0, 0, 1, 1)

        self.lineEdit_3 = QLineEdit(self.groupBoxBuildOptions)
        self.lineEdit_3.setObjectName(u"lineEdit_3")
        self.lineEdit_3.setText(u"")

        self.gridLayout_2.addWidget(self.lineEdit_3, 2, 1, 1, 1)

        self.label_2 = QLabel(self.groupBoxBuildOptions)
        self.label_2.setObjectName(u"label_2")
        self.label_2.setText(u"CC")

        self.gridLayout_2.addWidget(self.label_2, 1, 0, 1, 1)

        self.lineEdit_2 = QLineEdit(self.groupBoxBuildOptions)
        self.lineEdit_2.setObjectName(u"lineEdit_2")
        self.lineEdit_2.setText(u"")

        self.gridLayout_2.addWidget(self.lineEdit_2, 1, 1, 1, 1)

        self.label_3 = QLabel(self.groupBoxBuildOptions)
        self.label_3.setObjectName(u"label_3")
        self.label_3.setText(u"LINK")

        self.gridLayout_2.addWidget(self.label_3, 2, 0, 1, 1)

        self.lineEdit = QLineEdit(self.groupBoxBuildOptions)
        self.lineEdit.setObjectName(u"lineEdit")
        self.lineEdit.setText(u"")

        self.gridLayout_2.addWidget(self.lineEdit, 0, 1, 1, 1)


        self.gridLayout_3.addWidget(self.groupBoxBuildOptions, 2, 0, 1, 3)

        self.verticalSpacer_2 = QSpacerItem(20, 5, QSizePolicy.Minimum, QSizePolicy.Minimum)

        self.gridLayout_3.addItem(self.verticalSpacer_2, 3, 2, 1, 1)

        self.horizontalSpacer = QSpacerItem(722, 20, QSizePolicy.Expanding, QSizePolicy.Minimum)

        self.gridLayout_3.addItem(self.horizontalSpacer, 4, 0, 1, 1)

        self.groupBox_2 = QGroupBox(Dialog)
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


        self.retranslateUi(Dialog)

        QMetaObject.connectSlotsByName(Dialog)
    # setupUi

    def retranslateUi(self, Dialog):
        pass
    # retranslateUi

