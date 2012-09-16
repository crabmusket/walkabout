<?php
beginModule('walkabout');

	addProjectDefine("TORQUE_WALKABOUT_ENABLED");
	addEngineSrcDir("walkabout");
	addEngineSrcDir("walkabout/recast");

endModule();
?>