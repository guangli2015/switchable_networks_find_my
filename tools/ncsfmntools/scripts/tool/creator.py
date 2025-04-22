#
# Copyright (c) 2025 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

from .. import tool_type as tool_type

from . import core as tools_core
from . import nrfjprog_utils as nrfjprog
from . import nrfutil_utils as nrfutil

def tool_instance_create(tool):
    tool_instances = {
        tool_type.ToolType.nrfjprog: nrfjprog.ToolNrfjprog,
        tool_type.ToolType.nrfutil: nrfutil.ToolNrfutil
    }

    if tool not in tool_instances.keys():
        raise ValueError(f"Invalid tool type: {tool}")

    instance = tool_instances[tool]()

    assert isinstance(instance, tools_core.Tool)

    return instance
