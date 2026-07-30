# Verknuepft <exe-dir>/assets per NTFS-Junction mit dem echten assets/-Ordner, statt ihn zu
# kopieren. Ohne das wuerde z.B. PlacementSystem::SaveRotations() nur in eine Kopie neben der
# exe schreiben statt in die tatsaechliche Quelldatei - genau das ist frueher passiert.
# Erwartet TARGET_ASSETS_DIR und SOURCE_ASSETS_DIR als -D Variablen.
if(EXISTS "${TARGET_ASSETS_DIR}")
    file(REMOVE_RECURSE "${TARGET_ASSETS_DIR}")
endif()
execute_process(COMMAND powershell -NoProfile -Command
    "New-Item -ItemType Junction -Path '${TARGET_ASSETS_DIR}' -Target '${SOURCE_ASSETS_DIR}' -Force | Out-Null")
