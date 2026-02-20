#pragma once

#ifdef __cplusplus
namespace PrdUi {

void init(void);
void tick(void);
bool isInitialized(void);
void storageSelfTest(void);
void storageReadDump(void);

} // namespace PrdUi
#endif
