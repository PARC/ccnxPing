
AC_DEFUN([AX_GENERATE_ABOUT], [

	AC_PATH_PROG(GENERATE_ABOUT_BIN, longbow-generate-about, error, [$DEPENDENCY_DIR/bin:$PATH])

	if test xerror = x${GENERATE_ABOUT_BIN}; then
  		AC_MSG_ERROR(["Can't find longbow-generate-about, make sure Foundation is in the path. 
		(for example PATH=\$PATH:/usr/local/parc/bin)"])
	fi
])
