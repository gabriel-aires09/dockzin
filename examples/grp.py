import grp

grupo = grp.getgrnam("sudo")
print(grupo.gr_name)
print(grupo.gr_gid)
for membro in grupo.gr_mem:
    print(membro)
