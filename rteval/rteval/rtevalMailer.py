#
#   rtevalmailer.py - module for sending e-mails
#
#   Copyright 2009,2010   David Sommerseth <davids@redhat.com>
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
#
#   For the avoidance of doubt the "preferred form" of this code is one which
#   is in an open unpatent encumbered format. Where cryptographic key signing
#   forms part of the process of creating an executable the information
#   including keys needed to generate an equivalently functional executable
#   are deemed to be part of the source code.
#

import smtplib
import email


class rtevalMailer(object):
    "rteval mailer - sends messages via an SMTP server to designated e-mail addresses"

    def __init__(self, cfg):
        # this configuration object needs to have the following attributes set:
        # * smtp_server
        # * from_address
        # * to_address
        #
        errmsg = ""
        if not cfg.has_key('smtp_server'):
            errmsg = "\n** Missing smtp_server in config"
        if not cfg.has_key('from_address'):
            errmsg += "\n** Missing from_address in config"
        if not cfg.has_key('to_address'):
            errmsg += "\n** Missing to_address in config"

        if not errmsg == "":
            raise LookupError(errmsg)

        self.config = cfg


    def __prepare_msg(self, subj, body):
        msg = email.MIMEText.MIMEText(body)
        msg['subject'] = subj;
        msg['From'] = "rteval mailer <" + self.config.from_address+">"
        msg['To'] = self.config.to_address
        return msg


    def SendMessage(self, subject, body):
        "Sends an e-mail to the configured mail server and recipient"

        msg = self.__prepare_msg(subject, body)
        srv = smtplib.SMTP()
        srv.connect(self.config.smtp_server)
        srv.sendmail(self.config.from_address, self.config.to_address, str(msg))
        srv.close()

