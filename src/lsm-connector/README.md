lsm-connector
=============

Summary
-------
The lsm-connector module provides interfaces for communicating with LSM for GAV.

Description
-----------
* The lsm-connector module is a sub-module of render connector.
* The lsm-connector module takes charge of communicating with LSM for GAV.
* The lsm-connector module provides these interfaces.
1. Registering video/media window ID
2. Attaching punch through
3. Detaching punch through
4. Attaching surface
5. Detaching surface

Reference
---------
http://collab.lge.com/main/pages/viewpage.action?pageId=879424980

Test
----
### Punch through
stop sam
./lsm_connector_exporter &
./lsm_connector_importer $windowID PunchThrough
### Texture rendering
stop sam
./lsm_connector_exporter &
./lsm_connector_importer $windowID Surface

Dependencies
============
- wayland
- luna-surfacemanager-extensions (>=1.0.0-24.1.seetv.1)
- gpu-libs(For test tool : lsm_connector_exporter, lsm_connector_importer)


