<?php
beginModule('walkabout');

	addProjectDefine("TORQUE_WALKABOUT_ENABLED");
	addProjectDefine("TORQUE_WALKABOUT_EXTRAS_ENABLED");
	addEngineSrcDir("walkabout");
	addEngineSrcDir("walkabout/recast");

endModule();
?>