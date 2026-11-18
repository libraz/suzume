"""The MCP server instance every tool module registers its tools on.

It lives apart from ``server``, which imports every tool module, so that a tool
module can be imported on its own without re-entering that discovery.
"""

from mcp.server.mcpserver import MCPServer

mcp = MCPServer("suzume")
