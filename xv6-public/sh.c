#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>

// Simplifed xv6 shell.

#define MAXARGS 10

// All commands have at least a type. Have looked at the type, the code
// typically casts the *cmd to some specific cmd type.
struct cmd {
  int type;          //  ' ' (exec), | (pipe), '<' or '>' for redirection, ';' for sequential execution, '&' for parallel execution
};

struct execcmd {
  int type;              // ' '
  char *argv[MAXARGS];   // arguments to the command to be exec-ed
};

struct redircmd {
  int type;          // < or > 
  struct cmd *cmd;   // the command to be run (e.g., an execcmd)
  char *file;        // the input/output file
  int mode;          // the mode to open the file with
  int fd;            // the file descriptor number to use for the file
};

struct semicmd {
  int type;            // ;
  struct cmd *cmd;     // current command(s) to be executed
  struct cmd *nextCmd; // command(s) to be executed next
};
// we are using semicmd for & too except we are removing the wait so the scheduler can schedule parallel processes

struct pipecmd {
  int type;          // |
  struct cmd *left;  // left side of pipe
  struct cmd *right; // right side of pipe
};

int fork1(void);  // Fork but exits on failure.
struct cmd *parsecmd(char*);
char mostRecentOp = ';'; // most recent operator used, default is ;

// Execute cmd.  Never returns.
void
runcmd(struct cmd *cmd)
{
  int p[2], r, pid, pid2, wstatus;
  struct execcmd *ecmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;
  struct cmd *head, *tail, *temp;

  if(cmd == 0)
    exit(0);

  ecmd = (struct execcmd*)cmd;
  //fprintf(stderr,"---------\nType: %d\n",cmd->type);
  //fprintf(stderr,"Command: %s  \n",ecmd->argv[0]);
  //fprintf(stderr,"Arg(s): %s\n",ecmd->argv[1]);
  
  //if(cmd->type == 32 && ecmd->argv[0] == NULL){
    //fprintf(stderr,"error: cannot end with &, command must follow");
    //exit(0);
  //}

  //if(ecmd->argv[0] == NULL){
    //exit(0); // no need to execute a null command
  //}

  switch(cmd->type){
  default:
    fprintf(stderr, "unknown runcmd\n");
    exit(-1);

  case ' ':
    ecmd = (struct execcmd*)cmd;
    // The paths below were only for testing commands, we moved to the xv6 folder where we only use xv6 commands.
    // char path_1[100] = "/bin/";
    // char path_2[100] = "/usr/bin/";

    if(ecmd->argv[0] == 0)
      exit(0);

    //printf("ecmd->argv[0] = %s | ecmd->argv = %s\n", ecmd->argv[0], ecmd->argv[1]); -- command and first param (FOR TESTING ONLY)
    execvp(ecmd->argv[0], ecmd->argv); // runs the command in the current xv6 directory with the args list

    // if you want to use the linux code bin (which we will not do), here's how to do it:
    // execv(strcat(path_1, ecmd->argv[0]), ecmd->argv); // searching /bin/
    // execv(strcat(path_2, ecmd->argv[0]), ecmd->argv); // searching /usr/bin/

    fprintf(stderr, "exec failed: invalid xv6 command\n");
    break;

  case ';':
    head = cmd;
    while(head != 0){
      while(head->type == '&'){
        pid = fork1();

        if(pid < 0){
          fprintf(stderr,"fork failed\n");
          exit(1);
        }else if(pid == 0){
          runcmd(((struct semicmd*)head)->cmd); // run command
        }else{
          temp = head;
          head = ((struct semicmd*)head)->nextCmd;

          if(head->type != '&'){ // wait only if we hit a semicolon
            wait(&wstatus);
          }
        }

        free(temp);
      }

      while(head->type == ';'){
        pid = fork1();

        if(pid < 0){
          fprintf(stderr,"fork failed\n");
          exit(1);
        }else if(pid == 0){
          runcmd(((struct semicmd*)head)->cmd); // run command
        }else{
          wait(&wstatus);
          temp = head;
          head = ((struct semicmd*)head)->nextCmd;
        }

        free(temp);
      }
    }
    break;

  case '&':
    head = cmd;
    while(head != 0){
      while(head->type == '&'){
        pid = fork1();

        if(pid < 0){
          fprintf(stderr,"fork failed\n");
          exit(1);
        }else if(pid == 0){
          runcmd(((struct semicmd*)head)->cmd); // run command
        }else{
          temp = head;
          head = ((struct semicmd*)head)->nextCmd;

          if(head->type != '&'){ // wait only if we hit a semicolon
            wait(&wstatus);
          }
        }

        free(temp);
      }

      while(head->type == ';'){
        pid = fork1();

        if(pid < 0){
          fprintf(stderr,"fork failed\n");
          exit(1);
        }else if(pid == 0){
          runcmd(((struct semicmd*)head)->cmd); // run command
        }else{
          wait(&wstatus);
          temp = head;
          head = ((struct semicmd*)head)->nextCmd;
        }

        free(temp);
      }
    }
    break;

  case '>':
  case '<':
    rcmd = (struct redircmd*)cmd;
    fprintf(stderr, "redir not implemented\n");
    // Your code here ...
    runcmd(rcmd->cmd);
    break;

  case '|':
    pcmd = (struct pipecmd*)cmd;
    fprintf(stderr, "pipe not implemented\n");
    // Your code here ...
    break;
  }    
  exit(0);
}

int
getcmd(char *buf, int nbuf)
{
  
  if (isatty(fileno(stdin)))
    fprintf(stdout, "$ ");
  memset(buf, 0, nbuf);
  fgets(buf, nbuf, stdin);
  if(buf[0] == 0) // EOF
    return -1;
  return 0;
}

int
main(void)
{
  static char buf[100];
  int fd, r;

  // Read and run input commands.
  while(getcmd(buf, sizeof(buf)) >= 0){
    if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
      // Clumsy but will have to do for now.
      // Chdir has no effect on the parent if run in the child.
      buf[strlen(buf)-1] = 0;  // chop \n
      if(chdir(buf+3) < 0)
        fprintf(stderr, "cannot cd %s\n", buf+3);
      continue;
    }
    // buf is all of the input we typed into the prompt.
    if(fork1() == 0)
      runcmd(parsecmd(buf)); // calls to parsecmd first then to runcmd.
    wait(&r);
  }
  exit(0);
}

int
fork1(void)
{
  int pid;
  
  pid = fork();
  if(pid == -1)
    perror("fork");
  return pid;
}

struct cmd*
execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = ' ';
  return (struct cmd*)cmd;
}

struct cmd*
redircmd(struct cmd *subcmd, char *file, int type)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = type;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->mode = (type == '<') ?  O_RDONLY : O_WRONLY|O_CREAT|O_TRUNC;
  cmd->fd = (type == '<') ? 0 : 1;
  return (struct cmd*)cmd;
}

struct cmd*
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = '|';
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>;&";

int
gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;
  
  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  if(q)
    *q = s;
  ret = *s;
  switch(*s){
  case 0:
    break;
  case '|':
  case '<':
    s++;
    break;
  case '>':
    s++;
    break;
  case ';':
    s++;
    break;
  case '&':
    s++;
    break;
  default:
    ret = 'a';
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if(eq)
    *eq = s;
  
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

int
peek(char **ps, char *es, char *toks)
{
  char *s;
  
  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);
struct cmd *parsesemi(char**, char*);

// make a copy of the characters in the input buffer, starting from s through es.
// null-terminate the copy to make it a string.
char 
*mkcopy(char *s, char *es)
{
  int n = es - s;
  char *c = malloc(n+1);
  assert(c);
  strncpy(c, s, n);
  c[n] = 0;
  return c;
}

struct cmd*
parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es); // calls parseline
  peek(&s, es, ""); // calls peek
  if(s != es){
    fprintf(stderr, "leftovers: %s\n", s);
    exit(-1);
  }

  return cmd;
}

struct cmd*
parseline(char **ps, char *es)
{
  struct cmd *cmd;
  cmd = parsesemi(ps, es); // parseamp2(ps, es) and parsesemi(ps, es);
  return cmd;
}

struct cmd*
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd*
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while(peek(ps, es, "<>")){
    tok = gettoken(ps, es, 0, 0);
    if(gettoken(ps, es, &q, &eq) != 'a') {
      fprintf(stderr, "missing file for redirection\n");
      exit(-1);
    }
    switch(tok){
    case '<':
      cmd = redircmd(cmd, mkcopy(q, eq), '<');
      break;
    case '>':
      cmd = redircmd(cmd, mkcopy(q, eq), '>');
      break;
    }
  }
  return cmd;
}

struct cmd*
parsesemi(char **ps, char *es)
{
  struct cmd *cmd, *retc;
  struct semicmd *head, *tail, *temp;
  size_t size_semicmd;
  int pk;
  
  //fprintf(stderr,"ps:%s \nes:%s\n",*ps,es);
  //fprintf(stderr,"function parseamp called\n");
  cmd = parsepipe(ps, es); 
  //fprintf(stderr,"ps:%s \nes:%s\n",*ps,es);
  //fprintf(stderr,"cmd:%d\n",cmd->type);

  size_semicmd = sizeof(struct semicmd);
  head = (struct semicmd *) malloc(size_semicmd);
  
  //testing to see how peek works
  struct execcmd *ecmd11;
  ecmd11 = (struct execcmd*)cmd;
  //fprintf(stderr,"cmd: %s %s\n",ecmd11->argv[0],ecmd11->argv[1]);
  //fprintf(stderr,"ps: %s\n",*ps);
  //fprintf(stderr,"peek (;): %d\n",peek(ps,es,";"));
  //fprintf(stderr,"peek (&): %d\n",peek(ps,es,"&"));

  // This is to ensure that in cmd2 is seen as a & command in cmd1&cmd2;cmd3
  if(peek(ps,es,"&")) {
    head->type = '&';
    mostRecentOp = '&';
  } else {
    if(mostRecentOp == '&'){
      // this is to make sure there are arguments on both sides of all &.
      if(ecmd11->argv[0] == NULL){
        fprintf(stderr,"failed to exec: can't terminate with a &\n");
        exit(0);
      }   

      // otherwise, carry on.   
      head->type = '&';
    } else {
      head->type = ';';
    }
    mostRecentOp = ';';
  }

  //head->type = ';'; // change to & to test parallel
  head->cmd = cmd;
  head->nextCmd = 0;

  tail = head;
  retc = (struct cmd *)tail;

  if(peek(ps, es, ";") || peek(ps, es, "&")){ // change condition to just pk to test individual operators
    gettoken(ps, es, 0, 0);
    temp = (struct semicmd*)parsesemi(ps, es);
    tail->nextCmd = (struct cmd*) temp;
    tail = temp;
  }
  
  return retc;
}

struct cmd*
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;
  
  ret = execcmd();
  cmd = (struct execcmd*)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while(!peek(ps, es, "|;&")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0)
      break;
    if(tok != 'a') {
      fprintf(stderr, "syntax error\n");
      exit(-1);
    }
    cmd->argv[argc] = mkcopy(q, eq);
    argc++;
    if(argc >= MAXARGS) {
      fprintf(stderr, "too many args\n");
      exit(-1);
    }
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  return ret;
}
