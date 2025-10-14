#include <grp.h>
#include <stdio.h>

int main() {
  struct group *grp;

  // buscar grupo pelo nome
  grp = getgrnam("sudo");
  if (grp != NULL){
    printf("Nome: %s\n", grp->gr_name);
    printf("%s\n", grp->gr_passwd);

    for (int i = 0; grp->gr_mem[i] != NULL; i++){
      printf(" - %s\n", grp->gr_mem[i]);
    }
  }
  return 0;
}
