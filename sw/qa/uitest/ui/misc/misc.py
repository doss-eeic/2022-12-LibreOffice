# -*- tab-width: 4; indent-tabs-mode: nil; py-indent-offset: 4 -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

"""Covers sw/source/ui/misc/ fixes."""

from uitest.framework import UITestCase
from uitest.uihelper.common import type_text


class TestTmpdlg(UITestCase):
    def test_content_control_dialog(self):
        with self.ui_test.create_doc_in_start_center("writer") as xComponent:
            # Insert a dropdown content control, verify that a placeholder item is provided.
            self.xUITest.executeCommand(".uno:InsertDropdownContentControl")
            paragraphs = xComponent.Text.createEnumeration()
            paragraph = paragraphs.nextElement()
            portions = paragraph.createEnumeration()
            portion = portions.nextElement()
            contentControl = portion.ContentControl
            listItems = contentControl.ListItems
            self.assertEqual(len(listItems), 1)
            self.assertEqual(listItems[0][0].Name, "DisplayText")
            self.assertEqual(listItems[0][0].Value, "")
            self.assertEqual(listItems[0][1].Name, "Value")
            self.assertEqual(listItems[0][1].Value, "Choose an item")

            # Append a new list item.
            with self.ui_test.execute_dialog_through_command(".uno:ContentControlProperties") as xDialog:
                xAdd = xDialog.getChild("add")
                with self.ui_test.execute_blocking_action(xAdd.executeAction, args=('CLICK', ())) as xSubDialog:
                    xDisplayName = xSubDialog.getChild("displayname")
                    type_text(xDisplayName, "Foo Bar")
                    xValue = xSubDialog.getChild("value")
                    type_text(xValue, "foo-bar")

            # Verify that the UI appended the list item.
            listItems = contentControl.ListItems
            self.assertEqual(len(listItems), 2)
            self.assertEqual(listItems[1][0].Name, "DisplayText")
            self.assertEqual(listItems[1][0].Value, "Foo Bar")
            self.assertEqual(listItems[1][1].Name, "Value")
            self.assertEqual(listItems[1][1].Value, "foo-bar")


# vim: set shiftwidth=4 softtabstop=4 expandtab:
