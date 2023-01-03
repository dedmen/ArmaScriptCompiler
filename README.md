# ArmaScriptCompiler

## Config

Create `sqfc.json` in current working directory

```json
{
  "inputDirs": [
    "T:/x/",
    "T:/z/ace/",
    "T:/z/acex/",
    "T:/a3/"
  ],
  "includePaths": [
    "T:/"
  ],
  "excludeList": [
    "missions_f_contact",
    "missions_f_epa",
    "missions_f_oldman",
    "missions_f_tank",
    "missions_f_beta",
    "showcases\\showcase",
    "\\unitplay\\",
    "\\backups\\"
  ],
  "outputDir": "P:/",
  "workerThreads": 8
}
```

## Options
-l --log log file to output to 
  Example: .\ArmaScriptCompiler -l sqfc.log
