Gravity
=======

Gravity is Hemera's main middleware, and consists of a set of libraries and executables.

Gravity takes care of everything needed for running Hemera applications and services, and provides
DBus APIs for various lifecycle and commodity services. Its functionalities can be extended via
a plugin system which allows plugins to tap into Gravity's main functionalities directly, with additional
privileges and features.

Gravity is entirely written in Qt5/C++. Its APIs should be exposed by client libraries, such as Hemera Qt5 SDK,
and are not Qt dependent, as everything is exposed via a set of DBus interfaces.

Tree structure
--------------

* *lib*: Contains all of Gravity libraries
  * *Supermassive*: Supermassive is Gravity's main library. Most of middleware functionalities are implemented here.
  * *RemountHelper*: This library allows to build plugins for Gravity's filesystem remount infrastructure.
  * *Fingerprints*: This library allows to build plugins for Gravity's fingerprints infrastructure.
