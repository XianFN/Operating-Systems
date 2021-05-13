/* DECLARACIONES GLOBALES */

/* Librearías */
# include <stdio.h>
# include <stdlib.h>
# include <ctype.h>
# include <time.h>
# include <pthread.h>
# include <signal.h>
# include <sys/types.h>
# include <sys/wait.h>
# include <unistd.h>

/* Mutex */
pthread_mutex_t semaforoColaUsuarios; /* Semáforo para controlar la cola de usuarios. */
pthread_mutex_t semaforoFicheroLog; /* Semáforo para controlar la escritura en el log */
pthread_mutex_t semaforoControl; /* Semaforo para controlar lo del agente de seguridad */

/* Variables Condición */
pthread_cond_t controlAgente;
pthread_cond_t controlUsuario; /* Variable condición para sincronizar el agente de seguridad con el usuario */

/* Fichero Log */
FILE *fichero;
char *ficheroLog = "registroTiempos.log";

/* Estructura para cada usuario, de los que se creará una cola de 10 */
struct usuarios
{

	int idUsuario;
	int siendoAtendidoFacturador;
	int finalizadoFacturador;
	int siendoAtendidoAgente;
	int finalizadoAgente;
	int tipoUsuario;
	int dormido;
	pthread_t hiloUsuario;
};

struct usuarios usuario[10];

/* Estructura para los facturadores */
struct facturadores
{

	int idFacturador;
	int atendiendo;
	int usuariosAtendidos;
	pthread_t hiloFacturador;
};

struct facturadores facturador[2];

/* Estructura para el agente de seguridad */
struct agente
{

	int idAgente;
	int atendiendo;
	pthread_t hiloAgente;
};

struct agente agente;

/* Inicialización de variables */
int contadorUsuarios;
int finalizar;
int end;

/* Inicialización de funciones */
void nuevoUsuario(int sig);
void *accionesUsuario(void *idUsuario);
void *accionesFacturador(void *idFacturador);
void *agenteSeguridad(void *idAgente);
void finalizarPrograma();
int sacarPosicion(int identificadorUsuario);
int calculaAleatorios(int min, int max);
void writeLogMessage(char *id, char *msg);

int main(int argc, char *argv[])
{

	printf("\n\n------------------------------- AEROPUERTO NACIONAL CATALÁN CARLES PUIGDEMON -------------------------------\n\n");
	printf("Aeropuerto inicializado con 10 usuarios, 2 facturadores (uno para usuarios normales y otros para VIP) y un agente de seguridad\n");
	printf("Abra otro terminal en paralelo, escriba el comando 'ps -aux' y guarde el PID de la ejecución de este programa\n");
	printf("Escriba por el otro terminal 'kill -10 PID' para crear un usuario normal\n");
	printf("Escriba por el otro terminal 'kill -12 PID' para crear un usuario VIP\n");
	printf("Escriba por el otro terminal 'kill -2 PID' para finalizar el programa\n");

	struct sigaction u, f;
	u.sa_handler = nuevoUsuario;
	f.sa_handler = finalizarPrograma;

	/* 1.- 2.- MARCAMOS LAS SEÑALES QUE VA A RECIBIR nuevoUsuario */
	sigaction(SIGUSR1, &u, NULL);
	sigaction(SIGUSR2, &u, NULL);
	sigaction(SIGINT, &f, NULL);

	/* 3.- INICIALIZACIÓN DE RECURSOS */
	/* Semáforos */
	pthread_mutex_init(&semaforoColaUsuarios, NULL);
	pthread_mutex_init(&semaforoFicheroLog, NULL);
	pthread_mutex_init(&semaforoControl, NULL);

	/* Contador de usuarios */
	contadorUsuarios = 0;

	/* Variables para terminar el programa */
	finalizar = 0;
	end = 0;

	/* Inicializamos las variables de usuario */
	int i;

	for (i = 0; i < 10; i++)
	{

		usuario[i].idUsuario = 0;
		usuario[i].siendoAtendidoFacturador = 0;
		usuario[i].finalizadoFacturador = 0;
		usuario[i].siendoAtendidoAgente = 0;
		usuario[i].finalizadoAgente = 0;
		usuario[i].tipoUsuario = 0;
		usuario[i].dormido = 0;
	}

	/* Inicializando variables del facturador */
	for (i = 0; i < 2; i++)
	{
		int idF = i + 1;

		facturador[i].idFacturador = idF;
		facturador[i].atendiendo = 0;
		facturador[i].usuariosAtendidos = 0;
	}

	/* Inicializamos las variables del agente*/
	agente.idAgente = 1;
	agente.atendiendo = 0;

	/* Fichero Log */
	fichero = fopen("registroTiempos.log", "w");

	char aeropuerto[100];
	char apertura[100];
	sprintf(aeropuerto, "AEROPUERTO NACIONAL CATALÁN CARLES PUIGDEMON");
	sprintf(apertura, "Abrimos las puertas a los usuarios que vayan a embarcar");

	writeLogMessage(aeropuerto, apertura);

	/* Variable Condición */
	pthread_cond_init(&controlAgente, NULL);
	pthread_cond_init(&controlUsuario, NULL);

	/* 4.- Creamos 2 hilos facturadores */
	for (int i = 0; i < 2; i++)
	{

		pthread_create(&facturador[i].hiloFacturador, NULL, accionesFacturador, (void *)&facturador[i].idFacturador);
	}

	/* 5.- Crear hilo agente de seguridad */
	pthread_create(&agente.hiloAgente, NULL, agenteSeguridad, (void *)&agente.idAgente);

	/* 6.- 7.- Esperamos por señales de forma infinita, hasta que se termine el programa */
	printf("\n\tIntroduzca las señales que desee. Programa en ejecución...\n\n");

	while(finalizar == 0)
	{

		printf("A la espera de una señal...\t");
		pause();
		printf("Señal recibida\n");
	}

	/* Cuando no se permitan más señales, esperamos a que termine de ejecutar finalizarPrograma. En ese momento, se acaba el programa */
	while(end == 0)
	{

	}

	printf("\n\tFin del programa\n");

	char cierre[100];
	sprintf(cierre, "Cierra sus puertas hasta otro día");

	writeLogMessage(aeropuerto, cierre);

	return 0;
}

/* FUNCION nuevoUsuario(int sig). Recibe una señal y no devuelve nada. */
/* Esta función es una manejadora de señales, y permite crear a los usuarios y añadirlos a la cola. */
void nuevoUsuario(int sig)
{

	/* Bloqueamos para evitar que entren 2 o más usuarios a la vez. */
	pthread_mutex_lock(&semaforoColaUsuarios);

	int i = 0;
	int count = 0;

	while (i < 10)
	{

		/* COMPROBAMOS SI HAY ESPACIO EN LA COLA. */
		if (usuario[i].idUsuario == 0 && count == 0) /* SI HAY ESPACIO. */
		{

			/* SE AÑADE EL USUARIO. */
			count++;

			/* SE INCREMENTA EL CONTADOR DE USUARIOS. */
			contadorUsuarios++;

			/* EL ID DEL USUARIO PASA A SER EL DEL CONTADOR DE USUARIOS. */
			usuario[i].idUsuario = contadorUsuarios;

			/* USUARIO SIENDO ATENDIDO A 0. */
			usuario[i].siendoAtendidoFacturador = 0;
			usuario[i].siendoAtendidoAgente = 0;

			/* SELECCIONAMOS EL TIPO DE USUARIO DEPENDIENDO DE LA SEÑAL RECIBIDA. */
			switch(sig)
			{

				/* Si recibe la señal SIGUSR1, será tratado como un usuario normal. */
				case SIGUSR1:
							usuario[i].tipoUsuario = 1;
							break;

				/* Si recibe la señal SIGUSR2, será tratado como un usuario VIP. */
				case SIGUSR2:
							usuario[i].tipoUsuario = 2;
							break;
			}

			/* ESCRIBIMOS EN EL LOG QUE SE HA CREADO UN USUARIO. NO ESTÁ ESCRITO EN EL DISEÑO, PERO CABE DESTACAR ESTE EVENTO. */
			char id[21];
			sprintf(id, "nuevoUsuario");

			char msg[100];

			if (usuario[i].tipoUsuario == 1)
			{

				sprintf(msg, "El usuario_%d ha sido creado. Es un usuario normal.", usuario[i].idUsuario);
			}
			else
			{

				sprintf(msg, "El usuario_%d ha sido creado. Es un usuario VIP.", usuario[i].idUsuario);
			}

			writeLogMessage(id, msg);

			/* CREAMOS EL HILO DE EJECUCIÓN DEL USUARIO. */
			pthread_create(&usuario[i].hiloUsuario, NULL, accionesUsuario, (void *)&usuario[i].idUsuario);
		}
		else /* SI NO HAY ESPACIO. */
		{

			/* IGNORAMOS LA LLAMADA. */
		}

		i++;

		/* Desbloqueamos el mutex para que pueda entrar el siguiente usuario. */
		pthread_mutex_unlock(&semaforoColaUsuarios);
	}
}

/* FUNCIÓN accionesUsuario */
/* Función que va a realizar la ejecución del hilo de usuarios */
void *accionesUsuario(void *idUsuario){

	/* Pasamos a entero el id del usuario. */
	int identificadorUsuario = *(int *)idUsuario;
	int posicion = sacarPosicion(identificadorUsuario);

	char id[21];
	sprintf(id, "usuario_%d", identificadorUsuario);

	/* 1.- GUARDA EN EL LOG LA HORA DE ENTRADA */
	char entrada[100];
	sprintf(entrada, "Entra en el aeropuerto");

	writeLogMessage(id, entrada);

	char tipo[21];

	/* 2.- GUARDA EN EL LOG EL TIPO DE USUARIO */
	switch (usuario[posicion].tipoUsuario)
	{
		case 1:
				sprintf(tipo, "Usuario normal");

				writeLogMessage(id, tipo);

				break;

		case 2:
				sprintf(tipo, "Usuario VIP");

				writeLogMessage(id, tipo);

				break;
	}

	/* 3.- DUERME 4 SEGUNDOS */
	char duerme[100];
	sprintf(duerme, "Comienzo a dormir 4 segundos");

	writeLogMessage(id, duerme);

	pthread_mutex_lock (&semaforoColaUsuarios);
	usuario[posicion].dormido = 1;
	pthread_mutex_unlock(&semaforoColaUsuarios);

	sleep(4);

	pthread_mutex_lock (&semaforoColaUsuarios);
	usuario[posicion].dormido = 0;
	pthread_mutex_unlock(&semaforoColaUsuarios);

	char finDormir[100];
	sprintf(finDormir, "He terminado de dormir");

	writeLogMessage(id, finDormir);

	/* 4.- COMPRUEBA SI ESTÁ SIENDO ATENDIDO */
	//Bucle que vuelve a comprobar cada 3 segundos si el usuario se ha cansado, o tiene que ir al baño
	while(usuario[posicion].siendoAtendidoFacturador == 0 && usuario[posicion].finalizadoFacturador == 0)
	{

	    int espera = calculaAleatorios(1, 100);

	    /* 5.- SI NO LO ESTÁ, CALCULAMOS SU COMPROTAMIENTO */
		if(usuario[posicion].siendoAtendidoFacturador == 0 && usuario[posicion].finalizadoFacturador == 0)
		{

			//En el 20% de los casos los usuarios se cansan de esperan al facturador y se van
	    	if(espera >= 1 && espera <= 20)
	    	{

				/* Abandona el aeropuerto y lo escribe en el log */
				char esp[100];
	        	sprintf(esp, "El usuario_%d se ha cansado de esperar por la facturación y se va.", identificadorUsuario);

	        	writeLogMessage(id, esp);

	      		/* Liberamos espacio en la cola */
	      		pthread_mutex_lock (&semaforoColaUsuarios);

				usuario[posicion].idUsuario = 0;
				usuario[posicion].siendoAtendidoFacturador = 0;
				usuario[posicion].finalizadoFacturador = 0;
				usuario[posicion].siendoAtendidoAgente = 0;
				usuario[posicion].finalizadoAgente = 0;
				usuario[posicion].tipoUsuario = 0;

				pthread_mutex_unlock(&semaforoColaUsuarios);

	      		/* Terminamos el hilo */
	      		pthread_exit(NULL);
	    	}
	    	else if(espera >= 91 && espera <= 100)
	    	{

				char ba[100];
				sprintf(ba, "El usuario_%d se ha tenido que ir al baño.", identificadorUsuario);

				writeLogMessage(id, ba);

				/* Liberamos espacio en la cola */
				pthread_mutex_lock (&semaforoColaUsuarios);

				usuario[posicion].idUsuario = 0;
				usuario[posicion].siendoAtendidoFacturador = 0;
				usuario[posicion].finalizadoFacturador = 0;
				usuario[posicion].siendoAtendidoAgente = 0;
				usuario[posicion].finalizadoAgente = 0;
				usuario[posicion].tipoUsuario = 0;

				pthread_mutex_unlock(&semaforoColaUsuarios);

	      		/* Terminamos el hilo */
	      		pthread_exit(NULL);
			}
			else
			{

				sleep(3);
			}
	  	}
	}

	/* 6.- SI ESTÁ SIENDO ATENDIDO POR EL FACTURADOR, ESPERAMOS A QUE TERMINE */
	while(usuario[posicion].finalizadoFacturador == 0)
	{

	}

	/* 7.- CUANDO TERMINE */
	/* 7a.- ESPERA POR CONTROL */
	while(agente.atendiendo == 1) // Si está atendiendo a alguien, espera
	{

	}

	pthread_mutex_lock(&semaforoControl);

	/* 7b.-  LIBERA ESPACIO DE LA COLA */
	/*pthread_mutex_lock(&semaforoColaUsuarios);

	usuario[posicion].idUsuario = 0;
	usuario[posicion].siendoAtendidoFacturador = 0;
	usuario[posicion].finalizadoFacturador = 0;
	usuario[posicion].siendoAtendidoAgente = 0;
	usuario[posicion].finalizadoAgente = 0;
	usuario[posicion].tipoUsuario = 0;

	pthread_mutex_unlock(&semaforoColaUsuarios);*/

	/* 7c.- LIBERA EL CONTROL, Y SE QUEDA ESPERANDO A QUE PASE INSPECCIÓN */
	//Envía señal al agente de que está esperando a ser atendido por el
	char sig[100];
	sprintf(sig, "Se ha enviado la señal al agente de que esta esperando a ser atendido.");

	writeLogMessage(id, sig);

	pthread_cond_signal(&controlAgente);

	//Se queda a la espera de acabar de ser atendido por el agente
	pthread_cond_wait(&controlUsuario, &semaforoControl);

	char espAg[100];
	sprintf(espAg, "El usuario_%d está siendo atendido por el agente de seguridad.", identificadorUsuario);

	writeLogMessage(id, espAg);

	/* 7d.- CUANDO ACABA LIBERA EL CONTROL */
	// Hecho en agenteSeguridad

	/* 7e.- ESCRIBIMOS EN EL LOG QUE DEJA EL CONTROL */
	char dej[100];
  	sprintf(dej, "El usuario_%d deja el control de seguridad.", identificadorUsuario);

  	writeLogMessage(id, dej);

	/* 7f.- ESCIBE EN EL LOG QUE EMBARCA */
	char emb[100];
  	sprintf(emb, "El usuario_%d embarca en su avión.", identificadorUsuario);

  	writeLogMessage(id, emb);

	/* 7g.- LIBERA EL CONTROL */
	pthread_mutex_unlock(&semaforoControl);

	/* 8.- LIBERA ESPACIO EN LA COLA */
	pthread_mutex_lock (&semaforoColaUsuarios);

	usuario[posicion].idUsuario = 0;
	usuario[posicion].siendoAtendidoFacturador = 0;
	usuario[posicion].finalizadoFacturador = 0;
	usuario[posicion].siendoAtendidoAgente = 0;
	usuario[posicion].finalizadoAgente = 0;
	usuario[posicion].tipoUsuario = 0;

	pthread_mutex_unlock(&semaforoColaUsuarios);

	/* 9.- TERMINAMOS EL HILO. */
	pthread_exit(NULL);
}

/* FUNCIÓN accionesFacturador */
/* Función que va a permitir manejar la ejecución de los facturadores */
void *accionesFacturador(void *idFacturador){

	int identificadorFacturador = *(int *)idFacturador;

	facturador[identificadorFacturador - 1].atendiendo = 0;

	int usuarioElegido;

	char id[21];
	sprintf(id, "facturador_%d", identificadorFacturador);

	char entrada[100];

	if (identificadorFacturador == 1)
	{
		
		sprintf(entrada, "Mi prioridad son los usuarios normales.");
	}
	else
	{

		sprintf(entrada, "Mi prioridad son los usuarios VIP, aunque puedo atender a los normales si no hay nadie.");
	}

	writeLogMessage(id, entrada);

	/* 1.- BUSCAMOS EL USUARIO QUE MÁS TIEMPO LLEVE EN LA COLA */

	while(finalizar == 0)
	{

		int i, indice;

		while(facturador[identificadorFacturador - 1].atendiendo == 0)
		{

			pthread_mutex_lock(&semaforoColaUsuarios);

			/* Comenzamos buscando por la cola en la que tiene preferencia el facturador */
			for (int i = 0; i < 10; i++)
			{

				if (usuario[i].idUsuario > 0 && usuario[i].idUsuario < 99999 && usuario[i].tipoUsuario == identificadorFacturador && usuario[i].siendoAtendidoFacturador == 0 && usuario[i].finalizadoFacturador == 0 && usuario[i].dormido == 0)
				{

					/* Marcamos que el facturador ya no está disponible */
					facturador[identificadorFacturador - 1].atendiendo = 1;

					/* Guardamos el indice del array de usuarios */
					indice = i;

					/* Guardamos el id del usuario */
					usuarioElegido = usuario[i].idUsuario;

					pthread_mutex_unlock(&semaforoColaUsuarios);
				}
			}

			/* Si ha llegado hasta aquí sin haber encontrado a nadie en su cola, el VIP puede mirar en la cola de los usuarios normales */
			if (facturador[identificadorFacturador - 1].atendiendo == 0)
			{

				for (int i = 0; i < 10; i++)
				{

					/* Este sleep, sirve para casos en los que el facturador 2 ejecuta más rápido que el 1, y el usuario es normal, en ese caso los 2 entran a la vez. Por eso hay medio segundo de tiempo */
					sleep(0.2);

					if (0 < usuario[i].idUsuario && usuario[i].idUsuario < 99999 && identificadorFacturador == 2 && usuario[i].siendoAtendidoFacturador == 0 && usuario[i].finalizadoFacturador == 0 && usuario[i].dormido == 0)
					{

						/* Guardamos el indice del array de usuarios */
						indice = i;

						/* Guardamos el id del usuario */
						usuarioElegido = usuario[i].idUsuario;

						/* Marcamos que el facturador ya no está disponible */
						facturador[identificadorFacturador - 1].atendiendo = 1;

						pthread_mutex_unlock(&semaforoColaUsuarios);

					}
				}
			}

			pthread_mutex_unlock(&semaforoColaUsuarios);

			/* Si aun así no ha encontrado a nadie, duerme 1 y vuelve a buscar */
			if (facturador[identificadorFacturador - 1].atendiendo == 0){

						sleep(1);
			}
		}

		/* 2.- CALCULAMOS EL TIPO DE ATENCIÓN */

		int porcentaje= calculaAleatorios(1,100);
		int tiempo;
		char motivo[100];

		if (porcentaje >= 1 && porcentaje <= 80) /* 1-80 */
		{

			/* En el caso de que todo vaya correcto */
			tiempo = calculaAleatorios(1,4);
			sprintf(motivo, "El usuario_%d ha finalizado la facturacion con todo correcto. En el tiempo de %d", usuario[indice].idUsuario,tiempo);
		}
		else if (porcentaje > 80 && porcentaje <= 90) /* 81-90 */
		{

			/* En el caso de que se exceda de peso y tiene que pagar más */
			tiempo = calculaAleatorios(2,6);
			sprintf(motivo, "El usuario_%d ha finalizado la facturacion con exceso de peso. En el tiempo de %d", usuario[indice].idUsuario,tiempo);
		}
		else /* 91-100 */
		{

			/* En el caso de que no tenga el visado en regla */
			tiempo = calculaAleatorios(6,10);
			sprintf(motivo, "El usuario_%d ha finalizado la facturacion con el visado sin estar en regla. En el tiempo de %d", usuario[indice].idUsuario,tiempo);

		}

		/* 3.- CAMBIAMOS EL FLAG DEL USUARIO */
		pthread_mutex_lock(&semaforoColaUsuarios);
		usuario[indice].siendoAtendidoFacturador = 1;
		pthread_mutex_unlock(&semaforoColaUsuarios);

		/* 4.- GUARDAMOS EN EL LOG LA HORA DE INICIO DE LA ATENCIÓN */
		char inicio[100];
		sprintf(inicio, "Inicio facturacion del usuario_%d", usuario[indice].idUsuario);
		writeLogMessage(id, inicio);

		/* 5.- DORMIMOS EL TIEMPO DE ATENCIÓN */
		sleep(tiempo);

		/* 6.- GUARDAMOS EN EL LOG LA HORA DE FINALIZACIÓN DE LA ATENCIÓN */
		char fin[100];
		sprintf(fin, "Fin facuracion del usuario_%d.", usuario[indice].idUsuario);
		writeLogMessage(id, fin);

		/* 7.- GUARDAMOS EN EL LOG EL MOTIVO DE LA FINALIZACIÓN */
		writeLogMessage(id, motivo);

		/* 8.- CAMBIAMOS EL FLAG DEL USUARIO */
		pthread_mutex_lock(&semaforoColaUsuarios);
		usuario[indice].finalizadoFacturador = 1;
		pthread_mutex_unlock(&semaforoColaUsuarios);

		/* 9.- INCREMENTAMOS EL CONTADOR DE USUARIOS ATENDIDOS */
		facturador[identificadorFacturador - 1].usuariosAtendidos += 1;

		/* 10.- MIRA SI LE TOCA TOMAR CAFÉ */
		if (facturador[identificadorFacturador - 1].usuariosAtendidos % 5 == 0)
		{

			char inicioCafe[100];
			sprintf(inicioCafe, "Ya he atendido a 5 usuarios desde la última vez, asi que me voy a tomar un café");
			writeLogMessage(id, inicioCafe);

			sleep(10);

			char finCafe[100];
			sprintf(finCafe, "Ya he terminado de tomarme el café, volvemos al trabajo");
			writeLogMessage(id, finCafe);
		}

		/* Con esta sentencia, permitimos volver al paso 1 a buscar al siguiente */
		facturador[identificadorFacturador - 1].atendiendo = 0;
	}

	/* Terminamos el hilo */
	pthread_exit(NULL);
}

/* FUNCIÓN agenteSeguridad */
/* FUNCIÓN MANEJADORA DE THREADS, QUE NOS VA A PERMITIR MANEJAR TODO LO REQUERIDO CON EL AGENTE DE SEGURIDAD */
void *agenteSeguridad(void *idAgente)
{

	/* Pasamos a entero el id del agente. */
	int idAgenteSeguridad = *(int *)idAgente;

	char entradaAgente[21];
	sprintf(entradaAgente, "agenteSeguridad");

	/* Decimos que el agente está libre. */
	agente.atendiendo = 0;

	int usuarioElegido;

	/* 1.- TOMA EL MUTEX. */
	pthread_mutex_lock(&semaforoControl);

	while(finalizar == 0)
	{

		int i, indice;

		/* 2.- BUSCA UN USUARIO PARA ENTRAR AL CONTROL, SI NO LO HAY, ESPERA, SI NO, SE REALIZA SU ATENCIÓN. */
		while(agente.atendiendo == 0) /* Fin del bucle infinito. */
		{

			pthread_mutex_lock(&semaforoColaUsuarios);

			/* Bucle para buscar un usuario en la cola de usuarios. */
			for (i = 0; i < 10; i++)
			{

				/* Busca un usuario que tenga un id correcto, haya sido atendido y haya finalizado su atención por el facturador, y que no haya sido atendido ni haya finalizado su atención por el agente; además de que no debe estar atendiendo a nadie. */
				if (usuario[i].idUsuario != 0 && usuario[i].idUsuario < 99999 && usuario[i].siendoAtendidoFacturador == 1 && usuario[i].finalizadoFacturador == 1 && usuario[i].siendoAtendidoAgente == 0 && usuario[i].finalizadoAgente == 0)
				{

					usuarioElegido = usuario[i].idUsuario;
					indice = i;

					/* Marcamos que el usuario está siendo atendido por el agente de seguridad. */
					usuario[indice].siendoAtendidoAgente = 1;

					/* Marcamos que el agente de seguridad está ocupado. */
					agente.atendiendo = 1;

					char atender[100];
					sprintf(atender, "El usuario_%d ha llegado al control de seguridad", usuarioElegido);

					writeLogMessage(entradaAgente, atender);

					pthread_mutex_unlock(&semaforoColaUsuarios);
				}
			}

			if (agente.atendiendo == 0)
			{

				char espera[100];
				sprintf(espera, "Como no hay nadie para pasar el control de seguridad, voy a esperar a que alguien me llame");

				writeLogMessage(entradaAgente, espera);

				/* 3.- SI NO HAY NADIE, ESPERA A QUE UN USUARIO ESTÉ LIBRE Y LO LIBERE. CUANDO LO LIBEREN, VOLVERÁ AL FOR A BUSCAR A UN USUARIO. */
				pthread_cond_wait(&controlAgente, &semaforoControl);
			}
			else
			{

				/* Sale del bucle porque ya ha entrado un usuario al control. */
			}

			pthread_mutex_unlock(&semaforoColaUsuarios);
		}

		/* 4.- SI HAY ALGUIEN, ESCRIBE EN EL LOG Y CALCULA LA ATENCIÓN. */

		/* Guardamos en el log la hora de entrada de la atención. */
		char idUsuarioElegido[100];
		sprintf(idUsuarioElegido, "Atendiendo al usuario_%d.", usuarioElegido);

		writeLogMessage(entradaAgente, idUsuarioElegido);

		/* 5.- CALCULAMOS EL TIEMPO DE ATENCIÓN Y DUERME SEGÚN EL TIPO. */
		int atencion, dormir;

		atencion = calculaAleatorios(1, 100);

		if (atencion >= 1 && atencion <= 60) /* 1-60 */
		{

			dormir = calculaAleatorios(2, 3);

			sleep(dormir);
		}
		else /* 61-100 */
		{

			dormir = calculaAleatorios(10, 15);

			sleep(dormir);
		}

		/* 6.- ESCRIBE EN EL LOG LA HORA DE FINALIZACIÓN. */

		/* Guardamos en el log la hora de finalización de la atención. */
		char tiempoAtencion[100];
		sprintf(tiempoAtencion, "El tiempo de atención para el usuario_%d ha sido %d segundos.", usuarioElegido, dormir);

		writeLogMessage(entradaAgente, tiempoAtencion);

		/* Cambiamos el flag de atendido por el agente del usuario. */
		pthread_mutex_lock(&semaforoColaUsuarios);
		usuario[indice].finalizadoAgente = 1;

		pthread_mutex_unlock(&semaforoColaUsuarios);

		char terminaAtencion[100];
		sprintf(terminaAtencion, "Ha terminado la atención del usuario_%d por parte del Agente de Seguridad y por tanto puede embarcar en el avión.", usuarioElegido);

		writeLogMessage(entradaAgente, terminaAtencion);

		/* 7.- AVISAMOS AL USUARIO DEL FINAL DE LA ATENCIÓN, PARA ELLO LIBERAMOS LA VARIABLE CONDICIÓN DEL USUARIO. */
		pthread_cond_signal(&controlUsuario);

		/* Cambiamos el flag del agente para que pueda volver a atender */
		agente.atendiendo = 0;

		/* 8.- LIBERA EL MUTEX. */
		pthread_mutex_unlock(&semaforoControl);
	}

	/* TERMINAMOS EL HILO. */
	pthread_exit(NULL);
}

/* FUNCIÓN finalizarPrograma */
/* Esta función va a permitir finalizar el programa correctamente */
void finalizarPrograma()
{

	int i;

	/* Hacemos que los facturadores y el agente de seguridad realizen su último trabajo. No se permiten más señales */
	finalizar = 1;

	/* Terminamos la ejecución de todos los facturadores */
	for (int i = 0; i < 2; i++)
	{

		/* Si el facturador está atendiendo a alguien, espera hasta que no lo esté */
		while (facturador[i].atendiendo == 1)
		{

		}

		/* Guardamos en el log el número de usuarios que han facturado por cada cola */
		char id[21];
		char msg[100];
		sprintf(id, "facturador_%d", facturador[i].idFacturador);
		sprintf(msg, "He atendido a %d usuarios", facturador[i].usuariosAtendidos);

		writeLogMessage(id, msg);
	}

	/* Si el agente de seguridad está atendiendo a alguien, espera hasta que no lo esté */
	while (agente.atendiendo == 1)
	{

	}

	char idA[21];
	char msgA[100];
	sprintf(idA, "agenteSeguridad");
	sprintf(msgA, "He terminado la ejecución");

	writeLogMessage(idA, msgA);

	/* Cuando hayan terminado los facturadores y el agente, se eliminan los usuarios de la cola */
	for (int i = 0; i < 10; i++)
	{

		int user = usuario[i].idUsuario;

		/* Liberamos la cola */
		usuario[i].idUsuario = 0;
		usuario[i].siendoAtendidoFacturador = 0;
		usuario[i].finalizadoFacturador = 0;
		usuario[i].siendoAtendidoAgente = 0;
		usuario[i].finalizadoAgente = 0;
		usuario[i].tipoUsuario = 0;

		/* Termina la ejecución del hilo */
		pthread_cancel(usuario[i].hiloUsuario);

		if (user != 0)
		{

			char idU[21];
			char msgU[100];
			sprintf(idU, "usuario_%d", user);
			sprintf(msgU, "He terminado la ejecución");

			writeLogMessage(idU, msgU);
		}
	}

	/* Variable para indicar que debe terminar el programa */
	end = 1;
}

/* FUNCIÓN sacarPosicion */
/* Esta función permite obtener la posición en el array de usuarios dado un identificador */
int sacarPosicion(int identificadorUsuario){

	int i, posicion;

	for(i = 0; i < 10; i++)
	{

         if (usuario[i].idUsuario == identificadorUsuario)
         {

             posicion = i;
         }

    }

    return posicion;
}

/* FUNCIÓN  calculaAleatorios */
/* Función que permite calcular aleatorios en un margen */
int calculaAleatorios(int min, int max)
{

    srand(time(NULL));
    return rand() % (max-min+1) + min;
}

/* FUNCIÓN writeLogMessage */
/* Función que permite escribir en el fichero log */
void writeLogMessage(char *id, char *msg)
{

	pthread_mutex_lock(&semaforoFicheroLog);

	/* Calculamos la hora actual */
	time_t now = time(0);
	struct tm *tlocal = localtime(&now);
	char stnow[19];
	strftime(stnow, 19, "%d/%m/%y %H:%M:%S", tlocal);

	/* Escribimos en el log */
	fichero = fopen(ficheroLog, "a");
	fprintf(fichero, "[%s] %s: %s\n", stnow, id, msg);
	fclose(fichero);

	pthread_mutex_unlock(&semaforoFicheroLog);
}
