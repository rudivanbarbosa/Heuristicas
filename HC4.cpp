#include <ilcplex/ilocplex.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <stdio.h>
#include <time.h>
#include <set>
#include <vector>

typedef struct timespec timespec_t;
ILOSTLBEGIN
#define BILLION 1000000000L

/*-----------------------------------------------------------------------------------------------------------------------------------------
    Declaracao de variaveis
-------------------------------------------------------------------------------------------------------------------------------------------
*/
IloEnv env;
int i, j, p, n, t, alpha; // contadores
int N; // número total de pedidos
int J; // número total de produtos
int T; // número total de períodos
int aux = 0;
int aux2 =0;
double execTime;
double temporestante;
double TiLim = 100;
double atualFO;
double MelhorFO=-999999;

int myMax(int a, int b){
    if (a >= b) return a;
    else return b;
}

ILOMIPINFOCALLBACK2(logTimeCallback,timespec_t, startTime, const string, nome)
{
  FILE *arq;

arq = fopen(nome.c_str(), "a");

    // Tempo atual
    timespec_t currTime;
    clock_gettime(CLOCK_MONOTONIC, &currTime);

    // Tempo de execucao
    execTime = (currTime.tv_sec - startTime.tv_sec) + (double)(currTime.tv_nsec - startTime.tv_nsec)/BILLION;


    if (!hasIncumbent()) {
        fprintf(arq, " %.2f\n", execTime);
    } else {
        fprintf(arq, " %.2f %.f\n", execTime, getIncumbentObjValue());
    }
    fclose(arq);
}

int main(int argc, char **argv){


std::set<int> conjunto;
try{

        // leitura dos dados
        ifstream entrada(argv[1]);
        string nome(argv[2]);
        entrada >> J;
        entrada >> T;
        entrada >> N;


        //demanda do item j no pedido n
        IloArray<IloNumArray> q(env, J);
		for ( j=0; j<J; ++j)
		{
        q[j] = IloNumArray (env,N);
		}
            for (n = 0; n < N; n++)
                for (j = 0; j < J; j++)
                entrada >> q[j][n];



        //custo de troca da produção do item i para o item j
        IloArray<IloNumArray> sc(env, J);
		for ( i=0; i<J; ++i)
		{
        sc[i] = IloNumArray (env,J);
		}
        //tempo de setup da produção do item i para o item j
        IloArray<IloNumArray> st(env, J);
		for ( i=0; i<J; ++i)
		{
        st[i] = IloNumArray (env,J);
		}
            for(i = 0; i < J; i++){
                for( j = 0; j < J; j++){
                entrada >> sc[i][j];
                entrada >> st[i][j];
                    }
                        }

		//primeiro período da janela de entrega do pedido n
       IloNumArray F(env, N);
         //último período da janela de entrego do pedido n
       IloNumArray L(env, N);
        for( n = 0; n < N; n++){
        entrada >> F[n];
        entrada >> L[n];
                }

         //capacidade (tempo) de produção do período t
        IloNumArray C(env, T);
        for (t = 0; t < T; t++)
        entrada >> C[t];


        //tempo de produção do item j
       IloNumArray a(env, J);
        for (j = 0; j < J; j++)
        entrada >> a[j];

        //custo de estoque do item j por unidade
       IloNumArray h(env, J);
        for (j = 0; j < J; j++)
        entrada >> h[j];

        //lucro associado ao pedido n no período t
        IloArray<IloNumArray> P (env, N);
		for ( n=0; n<N; ++n)
		{
		P[n] = IloNumArray(env,T);
		}
            for (n = 0; n < N; n++){
				for (t = 0; t < T; t++)
					entrada >> P[n][t];
                            }


        //tempo máximo de estoque do item j
       IloNumArray sl(env, J);
        for (j = 0; j < J; j++)
        entrada >> sl[j];




        IloModel modelo1(env);


//// 1 se o pedido n vai ser atendido no período t e 0, caso contrário
        IloArray<IloBoolVarArray> gama(env, N);
		for ( n = 0 ; n < N ; n++ ){
			gama[n] = IloBoolVarArray(env, T);
                                }


        IloArray<IloNumArray> atualgama(env, N);
		for ( n = 0 ; n < N ; n++ ){
			atualgama[n] = IloNumArray(env, T);
                                }

        IloArray<IloNumArray> melhorgama(env, N);
		for ( n = 0 ; n < N ; n++ ){
			melhorgama[n] = IloNumArray(env, T);
                                }


IloArray< IloArray<IloNumVarArray> >  x(env, J);
            for ( j = 0 ; j < J ; j++ ){
			x[j] = IloArray<IloNumVarArray> (env, T);
                for ( t = 0 ; t < T ; t++ ){
                x[j][t] = IloNumVarArray(env, T,0,IloInfinity);
                }
                    }

    IloArray< IloArray<IloNumArray> >  atualx(env, J);
            for ( j = 0 ; j < J ; j++ ){
			atualx[j] = IloArray<IloNumArray> (env, T);
                for ( t = 0 ; t < T ; t++ ){
                atualx[j][t] = IloNumArray(env, T);
                }
                    }

            IloArray< IloArray<IloNumArray> >  melhorx(env, J);
            for ( j = 0 ; j < J ; j++ ){
			melhorx[j] = IloArray<IloNumArray> (env, T);
                for ( t = 0 ; t < T ; t++ ){
                melhorx[j][t] = IloNumArray(env, T);
                }
                    }

//// 1 se a máquina está preparada para a produção do item j no início do período t e 0, caso contrário
        IloArray<IloBoolVarArray> y(env, J);
		for ( j = 0 ; j < J ; j++ ){
			y[j] = IloBoolVarArray(env, T+1);
                                }

                IloArray<IloNumArray> atualy(env, J);
		for ( j = 0 ; j < J ; j++ ){
			atualy[j] = IloNumArray(env, T+1);
                                }

                IloArray<IloNumArray> melhory(env, J);
		for ( j = 0 ; j < J ; j++ ){
			melhory[j] = IloNumArray(env, T+1);
                                }

//// 1 se ocorre troca da produção do item i para o item j durante o período t e 0, caso contrário
IloArray< IloArray<IloBoolVarArray> >  z(env, J);
            for ( i = 0 ; i < J ; i++ ){
			z[i] = IloArray<IloBoolVarArray> (env, J);
                for ( j = 0 ; j < J ; j++ ){
                z[i][j] = IloBoolVarArray(env, T);
                }
                    }


                  IloArray< IloArray<IloNumArray> >  atualz(env, J);
            for ( i = 0 ; i < J ; i++ ){
			atualz[i] = IloArray<IloNumArray> (env, J);
                for ( j = 0 ; j < J ; j++ ){
                atualz[i][j] = IloNumArray(env, T);
                }
                    }


            IloArray< IloArray<IloNumArray> >  melhorz(env, J);
            for ( i = 0 ; i < J ; i++ ){
			melhorz[i] = IloArray<IloNumArray> (env, J);
                for ( j = 0 ; j < J ; j++ ){
                melhorz[i][j] = IloNumArray(env, T);
                }
                    }

////variável auxiliar que representa a ordem de produção do item j no período t.
            IloArray<IloNumVarArray> V(env, J);{
            for ( j = 0 ; j < J ; j++ )
            V[j] = IloNumVarArray(env, T, 0,IloInfinity);
            }


             IloArray<IloNumArray> atualV(env, J);{
            for ( j = 0 ; j < J ; j++ )
            atualV[j] = IloNumArray(env, T);
            }


            IloArray<IloNumArray> melhorV(env, J);{
            for ( j = 0 ; j < J ; j++ )
            melhorV[j] = IloNumArray(env, T);
            }




//Funcao objetivo
IloExpr fo1(env);

            for( n=0;n<N; n++){
                for( t= F[n];t<=L[n];t++){
                fo1 += P[n][t]*gama[n][t];
                            }
                                }


                    for ( j = 0 ; j < J ; j++ ){
                         for ( t = 0 ; t < T; t++ ){

                            for ( p=t+1 ; p< T  ; p++ ){
                                fo1 += - h[j]*(p-t)*x[j][t][p];
                                    }
                                        }
                                            }


                for ( t = 0 ; t < T ; t++ ){
                    for ( i = 0 ; i < J ; i++){
                        for ( j = 0 ; j < J ; j++){
                        fo1 += - sc[i][j]*z[i][j][t];
                        }
                            }
                                }

modelo1.add(IloMaximize(env, fo1));
fo1.end();


/////////////////////////////////////////////////////RESTRIÇÕES////////////////////////////////////////////////////

//Restrição 1

IloExpr Expr1(env);
IloExpr Expr2(env);
    for ( j = 0 ; j < J ; j++ ){
            for ( p = 0 ; p < T ; p++ ){

                for ( t = myMax(0,p-sl[j]) ; t <= p; t++ ){
                        Expr1 += x[j][t][p];
                                }

                    for ( n = 0 ; n < N ; n++ ){
                    Expr2 += q[j][n]*gama[n][p];
                    }
            modelo1.add(Expr1 == Expr2);
            Expr1.end();
            Expr1 = IloExpr(env);
            Expr2.end();
            Expr2 = IloExpr(env);
            }
                }

// Restrição 2
IloExpr Expr7(env);
        for ( t = 0 ; t < T ; t++ ){
			for ( i = 0 ; i < J ; i++ ){
				for ( j = 0 ; j < J ; j++ ){
						Expr7 += st[i][j]*z[i][j][t];
                                }
                        }
			for(j=0; j<J; j++){
                for(p=0; p<T; p++){
			     Expr7 += a[j]*x[j][t][p] ;
			     }
			}
                modelo1.add(Expr7 <= C[t] );
				Expr7.end();
				Expr7 = IloExpr(env);

        }
//   Restrição 3
IloExpr Expr8(env);
        for ( t = 0 ; t < T ; t++ ){
		for ( j = 0 ; j < J ; j++ ){
						Expr8 += y[j][t];
					}
						modelo1.add(Expr8 == 1);
						Expr8.end();
                        Expr8 = IloExpr(env);
                        }

// Restrição 4
IloExpr Expr9(env);
IloExpr Expr10(env);
		for ( t = 0 ; t < T ; t++ ){
		for ( j = 0 ; j < J ; j++ ){
            for ( i = 0 ; i < J ; i++ ){
            if(i!=j){
			Expr9 += z[i][j][t] ;
			Expr10 += z[j][i][t] ;
			}
			}
			modelo1.add(y[j][t] + Expr9 == Expr10 + y[j][t+1]);
            Expr9.end();
            Expr9 = IloExpr(env);
            Expr10.end();
			Expr10 = IloExpr(env);
		}

		}


//  Restrição 5
    IloExpr Expr13(env);
    IloExpr Expr14(env);
        for ( t = 0 ; t < T ; t++ ){
			for ( j = 0 ; j < J ; j++ ){
                    for ( i = 0 ; i < J ; i++ ){
                            if(j!=i){
							Expr13 += z[i][j][t];
                                    }
                                        }
                             for ( p = 0 ; p < T ; p++ ){
                            Expr14+= x[j][t][p] ;
                            }
                            for ( i = 0 ; i < J ; i++ ){
                            modelo1.add(Expr14 <= (C[t]/a[j])*(y[j][t] + Expr13) -(st[i][j]/a[j])*Expr13) ;
                                                    }
                                Expr13.end();
                                Expr13 = IloExpr(env);
                                Expr14.end();
                                Expr14 = IloExpr(env);
                                                    }


                                                        }
        //Restrição 6
        for ( t = 0 ; t < T ; t++ ){
			for ( i = 0 ; i < J ; i++ ){
			if(j!=i){
			for ( j = 0 ; j < J ; j++ ){

				modelo1.add(V[j][t] >= V[i][t] + 1 - J*(1 - z[i][j][t]));
                            }
			}
		}
		}
//Restrição 7
IloExpr Expr15(env);
        for ( n = 0 ; n < N ; n++ ){
						for( t = F[n] ; t <= L[n] ; t++ ){
							Expr15+= gama[n][t];
						}
						modelo1.add(Expr15 <= 1);
                        Expr15.end();
                        Expr15 = IloExpr(env);

					}

  //Restrição 8
  for ( n = 0 ; n < N ; n++ ){
  for ( t = 0 ; t < T ; t++ ){
            if(t< F[n] || t > L[n])
				modelo1.add(gama[n][t] == 0);
				}
				}


        IloCplex fase1(modelo1);
        fase1.extract(modelo1);
        fase1.setParam(IloCplex::Param::TimeLimit,TiLim);



        IloModel relax(env);
        relax.add(modelo1);

         for ( n = 0 ; n < N ; n++ ){
            for ( t = 0 ; t < T ; t++ ){
                relax.add(IloConversion(env, gama[n][t], ILOFLOAT));
                    }
                        }
            for ( j = 0 ; j < J ; j++ ){
            for ( t = 0 ; t < T+1 ; t++ ){
                relax.add(IloConversion(env, y[j][t], ILOFLOAT));
                    }
                        }
            for ( i = 0 ; i < J ; i++ ){
            for ( j = 0 ; j < J ; j++ ){
            for ( t = 0 ; t < T ; t++ ){
                relax.add(IloConversion(env, z[i][j][t], ILOFLOAT));
                    }
                        }
                            }
    IloCplex RELAX(relax);

    //Resolver o problema relaxado
    if(!RELAX.solve()){
    env.error() << "Nao se pode resolver" << endl;
    throw(-1);
    }

        int vetor[N];
        double VetorRC[T];
        double MelhoresRC[N];

        for (n= 0; n< N; n++){
            //Captura os custos reduzidos de todos períodos em cada pedido
            for (t= 0; t< T; t++){
                    VetorRC[t] = RELAX.getReducedCost(gama[n][t]);

                    }


                //Captura os melhores  custos reduzidos de cada pedido
                for (i=0; i<T; i++){
                   for(j=0; j<T; j++){
                        if(VetorRC[j] < VetorRC[i] ){
                            aux = VetorRC[j];
                            VetorRC[j] = VetorRC[i];
                            VetorRC[i] = aux;

                                    }
                                        }
                                            }

                                            MelhoresRC[n] = VetorRC[0];
                                            vetor[n] = n;

                                                    }
//Ordena os vetor dos custos reduzidos em forma não-crescente
    for (i=0; i<N; i++){
       for(j=0; j<N; j++){
            if(MelhoresRC[j] < MelhoresRC[i] ){
                aux = MelhoresRC[j];
                MelhoresRC[j] = MelhoresRC[i];
                MelhoresRC[i] = aux;
                aux2 = vetor[j];
                vetor[j] = vetor[i];
                vetor[i] = aux2;

                        }
                            }
                                }




    timespec_t startTime;
    clock_gettime(CLOCK_MONOTONIC, &startTime);
    fase1.use(logTimeCallback(env, startTime, nome));


//Heuristica - C4
for (int k=0; k< N && execTime <= 3600; k++){

        // Adicionar restricao no modelo fixada em 1

                IloExpr restricao1 (env);
                for(t=F[vetor[k]]; t<=L[vetor[k]]; t++){
                 restricao1 +=  gama[vetor[k]][t];
                    }

                IloConstraint adicionarum = restricao1 == 1;
                modelo1.add(adicionarum);
                conjunto.insert(vetor[k]);

         // Adicionar restricao fixando em zero

        IloConstraintArray adicionarzero(env,N);

        for(n=0; n< N; n++){

            IloExpr restricao0 (env);
                for(t=F[n]; t<=L[n]; t++){
                 restricao0 +=  gama[n][t];
                    }

                    adicionarzero[n] = restricao0 == 0;

                    if(conjunto.find(n) != conjunto.end()){  }
                        else{ modelo1.add(adicionarzero[n]); }

                            }



            //Se com a adicao da restricao o problema for factivel
            if(fase1.solve()){


            //Receber valor atual da variável y

            for(j= 0; j<J; j++){
                for(t= 0; t<T; t++){

                    atualy[j][t] = fase1.getValue(y[j][t]);

                    }
                        }

            //Receber valor atual da variável x
           for(j= 0; j<J; j++){
                for(t= 0; t<T; t++){
                    for(t= 0; t<T; t++){

                    atualx[j][t][t] = fase1.getValue(x[j][t][t]);

                    }
                        }
                            }

            //Receber valor atual da variável gama
            for(n= 0; n<N; n++){
                for(t= 0; t<T; t++){

                    atualgama[n][t] = fase1.getValue(gama[n][t]);

                    }
                        }

            //Receber valor atual da variável z
                for(i= 0; i<J; i++){
                    for(j= 0; j<J; j++){
                        for(t= 0; t<T; t++){

                            atualz[i][j][t] = fase1.getValue(z[i][j][t]);

                            }
                                }
                                    }



            //Comparar valor atual da solucao com o valor anterior
            atualFO = fase1.getObjValue();
            if(atualFO > MelhorFO){

            MelhorFO = atualFO;
            for(n=0; n<N; n++){ modelo1.remove(adicionarzero[n]);}

            //Receber valor da variável y da melhor solucao

            for(j= 0; j<J; j++){
                for(t= 0; t<T; t++){

                    melhory[j][t] = atualy[j][t];

                    }
                        }

            //Receber valor da variável x da melhor solucao
           for(j= 0; j<J; j++){
                for(t= 0; t<T; t++){
                    for(t= 0; t<T; t++){

                    melhorx[j][t][t] = atualx[j][t][t];

                    }
                        }
                            }

            //Receber valor da variável gama da melhor solucao
            for(n= 0; n<N; n++){
                for(t= 0; t<T; t++){

                    melhorgama[n][t] = atualgama[n][t];

                    }
                        }

            //Receber valor da variável z da melhor solucao
                for(i= 0; i<J; i++){
                    for(j= 0; j<J; j++){
                        for(t= 0; t<T; t++){

                            melhorz[i][j][t] = atualz[i][j][t];

                            }
                                }
                                    }



                                                }

            //Se a solucao piorar, remover a restricao adicionada
            else{


             for(n=0; n<N; n++){ modelo1.remove(adicionarzero[n]);}
             modelo1.remove(adicionarum);
             conjunto.erase(vetor[k]);

            }


                    }
            //Se o problema for infactivel, remover a restricao recem adicionada
            else{

              for(n=0; n<N; n++){ modelo1.remove(adicionarzero[n]); }
              modelo1.remove(adicionarum);
              conjunto.erase(vetor[k]);

                }


                    }


                            double tempo = execTime;
                            FILE *arq;
                            arq = fopen(argv[2], "a");
                            fprintf(arq, " Melhor solucao encontrada na FASE 1: %.f \n", MelhorFO);
                            fprintf(arq, " Tempo de execucao da FASE 1: %.2f \n", tempo);
                            fclose(arq);







} // fim do try

catch(IloException& ex) {
cerr << "ERRO: " << ex << endl;
}
catch(...) {
cerr << "ERRO!" << endl;
}

env.end();
return 0;

}
