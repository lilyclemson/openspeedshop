###############################################################################
# Copyright (c) 2006 Silicon Graphics, Inc. All Rights Reserved.
# Copyright (c) 2006-2010 Krell Institute. All Rights Reserved.
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 59 Temple
# Place, Suite 330, Boston, MA  02111-1307  USA
################################################################################

# directories that will be affected
SUBDIRS = users_guide cli_doc plugin_guide

DIST_SUBDIRS = users_guide cli_doc plugin_guide design_doc

EXTRA_DIST = help.adp

package = @PACKAGE@
packver = @PACKAGE@-@VERSION@
#docdir  = ${prefix}/share/doc/@PACKAGE@-@VERSION@
docdir  = ${prefix}/share/doc/packages/OpenSpeedShop

install-reference: help.adp
	@$(NORMAL_INSTALL)
	${mkinstalldirs} ${DESTDIR}${docdir}
	${INSTALL_DATA} help.adp ${DESTDIR}${docdir}

uninstall-reference: help.adp
	@$(NORMAL_UNINSTALL)
	-rm -rf ${DESTDIR}${docdir}

all-local: help.adp

install-data-am: install-reference

uninstall: uninstall-reference

