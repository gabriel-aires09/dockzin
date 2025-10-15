#define _GNU_SOURCE // habilita extensões GNU/Linux que não fazem parte do padrão POSIX. Necessário para usar clone() e unshare()
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <sched.h> // namespaces e controle de processos
#include <seccomp.h> // para filtro de chamadas de sistema 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/capability.h> // para gerenciar permissões granulares (capabilities) no Linux
#include <sys/mount.h> // para montar sistemas de arquivos isolads
#include <sys/prctl.h>
#include <sys/resource.h> // limitação de recursos (CPU, memória, etc)
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <linux/capability.h>
#include <linux/limits.h>

// Armazena a configuração do container que será criado
struct child_config {
  int argc; // Número de argumentos
  uid_t uid; // ID do usuário
  int fd; // file descriptor
  char *hostname; // Nome do host isolado do container
  char **argv; // Argumentos do comando a executar
  char *mount_dir; // Diretório raíz do container
}

// capabilities
// mounts
// syscalls
// resources
// child



/*
 * choose_hostname
 * Gera automaticamente o nome do host de forma aleatória, inspirada em cartas de tarô
 */
int choose_hostname(char *buff, size_t len){
  static const char *suits[] = { "swords", "wands", "pentacles", "cups"};
  static const char *minor[] = {
    "ace", "two", "three", "four", "five", "six", "seven", "eight",
    "nine", "ten", "page", "knight", "queen", "king"
  };
  static const char *major[] = {
    "fool", "magician", "high-priestess", "empress", "emperor",
    "hierophant", "lovers", "chariot", "strength", "hermit",
    "wheel", "justice", "hanged-man", "death", "temperance",
    "devil", "tower", "star", "moon", "sun", "judgment", "world"
  };
  struct timepesc now = {0};
  clock_gettime(CLOCK_MONOTONIC, &now);
  size_t ix = now.tv_nsec % 78;
  if (ix < sizeof(major) / sizeof(*major)){
    snprintf(buff, len, "%051x-%s", now.tv_sec, major[ix]);
  } else {
    ix -= sizeof(major) / sizeof(*major);
    snprintf(buff, len,
        "%051xc-%s-of-%s",
        now.tv_sec,
        minor[ix % (sizeof(minor) / sizeof(*minor))],
        suits[ix / (sizeof(minor) / sizeof(*minor))]
    );
  }
  return 0;
} 

/*
 * argc - quantidade de argumentos passados via terminal
 * argv - vetor com string de argumentos
 */
int main (int argc, char **argv){ 
  struct child_config config = {0}; // passa a configuracao ao processo que sera isolado no namespaces
  int err = 0; // flag de erro
  int option = 0;
  int sockets[2] = {0}; // usado para comunicacao entre processo pai e filho
  pid_t child_pid = 0; // guarda o process id do processo filho criado com clone() e fork()
  int last_optind = 0; // controle da posição de parsing de argumentos

  // Processa as opções da linha de comando
  while ((option = getopt(argc, argv, "c:m:u"))) {
    switch(option){
      case 'c': // comando a ser executado no container
        config.argrc = argc - last_optind - 1;
        config.argv = &argv[argc - config.argc];
        goto finish_options;
      case 'm': // diretorio root (via chroot ou mount --bind)
        config.mount_dir = optarg;
        break;
      case 'u': // user_id dentro do container
        if (sscanf(optarg, "%d", &config.uid) != 1){
          fprintf(stderr, "badly-formatted uid: %s\n", optarg);
          goto usage;
        }
        break;
      default:
        goto usage;
    }
    last_optind = optind;
  }

  finish_options:
    if (!config.argc) goto usage; // comando -c foi informado
    if (!config.mount_dir) goto usage; // diretorio de montagem especificado

  // check-linux-version
  fprintf(stderr, "=> validating Linux version...");
  struct utsname host = {0}; // declara uma estrutura utsname, usada para armazenar informações do sistema
  
  /*
   * uname() preenche o struct host com informações sobre o sistema
   * se retornar diferente de zero, retorna um erro
   * depois vai ao rótulo cleanup
   */
  
  if (uname(&host)) {
    fprintf(stderr, "failed %m\n");
    goto cleanup; // pula execução do código para uma label (rótulo ) específico
  }
  int major = -1;
  int minor = -1;

  // imprime uma mensagem 
  fprintf(stderr, "=> validating Linux version...");
  struct utsname host = {0};
  if (uname(&host)) {
    fprintf(stderr, "failed %m\n");
    goto cleanup; // pula execução do código para uma label (rótulo ) específico
  }
  int major = -1;
  int minor = -1;

  // imprime uma mensagem de erro, avisando que está validando a versão do linux
  if (sscanf(host.release, "%u.%u.", &major, &minor) != 2) {
    fprintf(stderr, "weird release format: %s\n", host.release);
    goto cleanup;
  }

  if (major != 4 || (minor != 7 && minor != 8)) {
    fprintf(stderr, "expected 4.7.x pr 4.8.x: %s\n", host.release);
    goto cleanup;
  }

  if (strcmp("x86_64", host.machine)) {
    fprintf(stderr, "expected x86_64: %s\n", host.machine);
    goto cleanup;
  }
  fprintf(stderr, "%s on %s. \n", host.release, host.machine);

  // criação do hostname do container
  char hostname[256] = {0};
  if (choose_hostname(hostname, sizeof(hostname))) // gera host para hostname dentro do container seja diferente do host
    goto error;
  config.hostname = hostname;

  // namespaces
  goto cleanup;

  // mostra os parametros do programa
  usage: 
    fprint(stderr, "Usage: %s -u -1 -m . -c /bin/sh ~\n", argv[0]);
  error: // mostra os erros
    err = 1;
  cleanup: // fecha recursos-sockets do container e retorna um erro
    if (sockets[0]) close(sockets[0]);
    if (sockets[1]) close(sockets[1]);
    return err;
}
