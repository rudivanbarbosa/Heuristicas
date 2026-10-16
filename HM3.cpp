#include <ilcplex/ilocplex.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <set>
#include <vector>

typedef struct timespec timespec_t;
ILOSTLBEGIN
#define BILLION 1000000000L

/*-----------------------------------------------------------------------------------------------------------------------------------------
    Cronometro: tempo (em segundos) desde a ultima chamada de reinicia()
-------------------------------------------------------------------------------------------------------------------------------------------
*/
class Cronometro {
    timespec_t inicio;

public:
    void reinicia(){
        clock_gettime(CLOCK_MONOTONIC, &inicio);
    }

    double decorrido() const {
        timespec_t agora;
        clock_gettime(CLOCK_MONOTONIC, &agora);
        return (agora.tv_sec - inicio.tv_sec) + (double)(agora.tv_nsec - inicio.tv_nsec)/BILLION;
    }
};

/*-----------------------------------------------------------------------------------------------------------------------------------------
    Instancia: dados do problema, lidos do arquivo
-------------------------------------------------------------------------------------------------------------------------------------------
*/
class Instancia {
public:
    int N; // numero total de pedidos
    int J; // numero total de produtos
    int T; // numero total de periodos

    IloArray<IloNumArray> q;  // q[j][n]: demanda do item j no pedido n
    IloArray<IloNumArray> sc; // sc[i][j]: custo de troca (preparacao da maquina) da producao do item i para o item j
    IloArray<IloNumArray> st; // st[i][j]: tempo de troca (preparacao da maquina) da producao do item i para o item j
    IloNumArray F;            // F[n]: primeiro periodo da janela de entrega do pedido n
    IloNumArray L;            // L[n]: ultimo periodo da janela de entrega do pedido n
    IloNumArray C;            // C[t]: capacidade (tempo) de producao do periodo t
    IloNumArray a;            // a[j]: tempo para producao de uma unidade do item j
    IloNumArray h;            // h[j]: custo de estoque do item j (por unidade e por periodo em estoque)
    IloArray<IloNumArray> P;  // P[n][t]: receita associada ao pedido n se ele for entregue no periodo t
    IloNumArray sl;           // sl[j]: tempo maximo de estoque do item j (shelf-life), em periodos

    Instancia(IloEnv env, const char *arquivo){
        ifstream entrada(arquivo);
        entrada >> J;
        entrada >> T;
        entrada >> N;

        q = IloArray<IloNumArray>(env, J);
        for (int j = 0; j < J; j++)
            q[j] = IloNumArray(env, N);
        for (int n = 0; n < N; n++)
            for (int j = 0; j < J; j++)
                entrada >> q[j][n];

        sc = IloArray<IloNumArray>(env, J);
        st = IloArray<IloNumArray>(env, J);
        for (int i = 0; i < J; i++){
            sc[i] = IloNumArray(env, J);
            st[i] = IloNumArray(env, J);
        }
        for (int i = 0; i < J; i++)
            for (int j = 0; j < J; j++){
                entrada >> sc[i][j];
                entrada >> st[i][j];
            }

        F = IloNumArray(env, N);
        L = IloNumArray(env, N);
        for (int n = 0; n < N; n++){
            entrada >> F[n];
            entrada >> L[n];
        }

        C = IloNumArray(env, T);
        for (int t = 0; t < T; t++)
            entrada >> C[t];

        a = IloNumArray(env, J);
        for (int j = 0; j < J; j++)
            entrada >> a[j];

        h = IloNumArray(env, J);
        for (int j = 0; j < J; j++)
            entrada >> h[j];

        P = IloArray<IloNumArray>(env, N);
        for (int n = 0; n < N; n++)
            P[n] = IloNumArray(env, T);
        for (int n = 0; n < N; n++)
            for (int t = 0; t < T; t++)
                entrada >> P[n][t];

        sl = IloNumArray(env, J);
        for (int j = 0; j < J; j++)
            entrada >> sl[j];
    }
};

/*-----------------------------------------------------------------------------------------------------------------------------------------
    Solucao: valores das variaveis que a heuristica usa para guardar a melhor solucao
-------------------------------------------------------------------------------------------------------------------------------------------
*/
class Solucao {
public:
    vector< vector<double> > gama;       // gama[n][t]
    vector< vector<double> > y;          // y[j][t]
    vector< vector< vector<double> > > z; // z[i][j][t]

    Solucao(const Instancia &d)
        : gama(d.N, vector<double>(d.T)),
          y(d.J, vector<double>(d.T)),
          z(d.J, vector< vector<double> >(d.J, vector<double>(d.T))) {}
};

/*-----------------------------------------------------------------------------------------------------------------------------------------
    Modelo: variaveis, funcao objetivo e restricoes do problema
-------------------------------------------------------------------------------------------------------------------------------------------
*/
class Modelo {
    IloEnv env;
    const Instancia &d;

public:
    // gama[n][t]: 1 se o pedido n vai ser atendido (entregue por inteiro, sem entrega parcial)
    // no periodo t e 0, caso contrario. Binaria.
    IloArray<IloBoolVarArray> gama;

    // x[j][t][p]: quantidade do item j produzida no periodo t para ser entregue no periodo p.
    // A idade do produto na entrega e p - t. Continua, x >= 0.
    IloArray< IloArray<IloNumVarArray> > x;

    // y[j][t]: 1 se a maquina esta preparada para a producao do item j no inicio do periodo t
    // e 0, caso contrario (o indice t+1 e usado na restricao de fluxo). Binaria.
    IloArray<IloBoolVarArray> y;

    // z[i][j][t]: 1 se ocorre troca da producao do item i para o item j durante o periodo t
    // e 0, caso contrario. Binaria.
    IloArray< IloArray<IloBoolVarArray> > z;

    // V[j][t]: variavel auxiliar que representa a ordem de producao do item j no periodo t;
    // usada para evitar subciclos desconexos nas trocas. Continua, V >= 0.
    IloArray<IloNumVarArray> V;

    Modelo(IloEnv e, const Instancia &inst) : env(e), d(inst) {
        gama = IloArray<IloBoolVarArray>(env, d.N);
        for (int n = 0; n < d.N; n++)
            gama[n] = IloBoolVarArray(env, d.T);

        x = IloArray< IloArray<IloNumVarArray> >(env, d.J);
        for (int j = 0; j < d.J; j++){
            x[j] = IloArray<IloNumVarArray>(env, d.T);
            for (int t = 0; t < d.T; t++)
                x[j][t] = IloNumVarArray(env, d.T, 0, IloInfinity);
        }

        y = IloArray<IloBoolVarArray>(env, d.J);
        for (int j = 0; j < d.J; j++)
            y[j] = IloBoolVarArray(env, d.T+1);

        z = IloArray< IloArray<IloBoolVarArray> >(env, d.J);
        for (int i = 0; i < d.J; i++){
            z[i] = IloArray<IloBoolVarArray>(env, d.J);
            for (int j = 0; j < d.J; j++)
                z[i][j] = IloBoolVarArray(env, d.T);
        }

        V = IloArray<IloNumVarArray>(env, d.J);
        for (int j = 0; j < d.J; j++)
            V[j] = IloNumVarArray(env, d.T, 0, IloInfinity);
    }

    IloModel construir(){
        IloModel modelo(env);

        //Funcao objetivo: maximizar o lucro, isto e,
        //   receitas dos pedidos atendidos:           soma de P[n][t]*gama[n][t], para t na janela [F[n], L[n]]
        // - custo de estoque (idade do produto):      soma de h[j]*(p-t)*x[j][t][p], para p > t
        //                                             (cada unidade produzida em t e entregue em p paga h[j] por periodo estocado)
        // - custos de preparacao da maquina:          soma de sc[i][j]*z[i][j][t] (dependentes da sequencia de producao).
        IloExpr fo(env);

        for (int n = 0; n < d.N; n++)
            for (int t = d.F[n]; t <= d.L[n]; t++)
                fo += d.P[n][t]*gama[n][t];

        for (int j = 0; j < d.J; j++)
            for (int t = 0; t < d.T; t++)
                for (int p = t+1; p < d.T; p++)
                    fo += - d.h[j]*(p-t)*x[j][t][p];

        for (int t = 0; t < d.T; t++)
            for (int i = 0; i < d.J; i++)
                for (int j = 0; j < d.J; j++)
                    fo += - d.sc[i][j]*z[i][j][t];

        modelo.add(IloMaximize(env, fo));
        fo.end();

        IloExpr Expr1(env);
        IloExpr Expr2(env);

        //Atendimento da demanda: producao dos itens que compoem os pedidos aceitos.
        //Para cada item j e periodo p, o que e entregue em p (soma dos x[j][t][p]) deve vir de producao feita
        //entre os periodos p - sl[j] e p (respeita o shelf-life) e ser igual a demanda de j nos pedidos
        //atendidos em p (soma de q[j][n]*gama[n][p]).
        for (int j = 0; j < d.J; j++){
            for (int p = 0; p < d.T; p++){
                int inicio = (int)(p - d.sl[j]);
                if (inicio < 0) inicio = 0;
                for (int t = inicio; t <= p; t++)
                    Expr1 += x[j][t][p];

                for (int n = 0; n < d.N; n++)
                    Expr2 += d.q[j][n]*gama[n][p];

                modelo.add(Expr1 == Expr2);
                Expr1.clear();
                Expr2.clear();
            }
        }

        // Capacidade: limite de capacidade produtiva de cada periodo t.
        // O tempo de producao (a[j]*x) mais o tempo de preparacao das trocas (st[i][j]*z) nao pode exceder C[t].
        for (int t = 0; t < d.T; t++){
            for (int i = 0; i < d.J; i++)
                for (int j = 0; j < d.J; j++)
                    Expr1 += d.st[i][j]*z[i][j][t];

            for (int j = 0; j < d.J; j++)
                for (int p = 0; p < d.T; p++)
                    Expr1 += d.a[j]*x[j][t][p];

            modelo.add(Expr1 <= d.C[t]);
            Expr1.clear();
        }

        //   Preparacao unica: em cada periodo t, a maquina esta preparada
        //   para um unico item no inicio do periodo (o primeiro a ser produzido).
        for (int t = 0; t < d.T; t++){
            for (int j = 0; j < d.J; j++)
                Expr1 += y[j][t];
            modelo.add(Expr1 == 1);
            Expr1.clear();
        }

        // Fluxo: o estado y[j][t] mais as trocas que chegam
        // ao item j e igual as trocas que saem de j mais o estado y[j][t+1] no inicio do periodo seguinte.
        // (No codigo as somas em i excluem i = j.)
        for (int t = 0; t < d.T; t++){
            for (int j = 0; j < d.J; j++){
                for (int i = 0; i < d.J; i++){
                    if (i != j){
                        Expr1 += z[i][j][t];
                        Expr2 += z[j][i][t];
                    }
                }
                modelo.add(y[j][t] + Expr1 == Expr2 + y[j][t+1]);
                Expr1.clear();
                Expr2.clear();
            }
        }

        //  Ligacao entre producao e preparacao: o item j so pode ser produzido
        //  em t se a maquina estiver preparada para ele (y[j][t] = 1) ou se houver troca para ele em t (soma de z[i][j][t]);
        //  o tempo de troca st[i][j] e descontado da capacidade C[t]. A constante C[t]/a[j] e o big-M.
        for (int t = 0; t < d.T; t++){
            for (int j = 0; j < d.J; j++){
                for (int i = 0; i < d.J; i++)
                    if (j != i)
                        Expr2 += z[i][j][t];

                for (int p = 0; p < d.T; p++)
                    Expr1 += x[j][t][p];

                for (int i = 0; i < d.J; i++)
                    modelo.add(Expr1 <= (d.C[t]/d.a[j])*(y[j][t] + Expr2) - (d.st[i][j]/d.a[j])*Expr2);

                Expr2.clear();
                Expr1.clear();
            }
        }

        //Ordem de producao (tipo MTZ): ordem dos itens em cada periodo.
        //Se ha troca de i para j em t (z[i][j][t] = 1), entao V[j][t] >= V[i][t] + 1, o que captura a ordem
        //de producao e evita subciclos desconexos.
        //OBS.: o codigo original tinha um "if(j!=i)" fora do laco de j, que usava o j global (sempre igual a J
        //nesse ponto), logo a condicao era sempre verdadeira e as restricoes sao criadas tambem para i == j.
        //O comportamento foi mantido.
        for (int t = 0; t < d.T; t++)
            for (int i = 0; i < d.J; i++)
                for (int j = 0; j < d.J; j++)
                    modelo.add(V[j][t] >= V[i][t] + 1 - d.J*(1 - z[i][j][t]));

        //Atendimento unico: cada pedido n pode ser atendido no maximo uma vez
        //durante o horizonte de planejamento (dentro da sua janela de entrega [F[n], L[n]]).
        for (int n = 0; n < d.N; n++){
            for (int t = d.F[n]; t <= d.L[n]; t++)
                Expr1 += gama[n][t];
            modelo.add(Expr1 <= 1);
            Expr1.clear();
        }

        //Janela de atendimento: gama[n][t] = 0 para t < F[n] ou t > L[n].
        for (int n = 0; n < d.N; n++)
            for (int t = 0; t < d.T; t++)
                if (t < d.F[n] || t > d.L[n])
                    modelo.add(gama[n][t] == 0);

        Expr1.end();
        Expr2.end();
        return modelo;
    }

    // Relaxacao linear: gama, y e z passam a ser continuas
    IloModel relaxar(IloModel base){
        IloModel relax(env);
        relax.add(base);

        for (int n = 0; n < d.N; n++)
            for (int t = 0; t < d.T; t++)
                relax.add(IloConversion(env, gama[n][t], ILOFLOAT));

        for (int j = 0; j < d.J; j++)
            for (int t = 0; t < d.T+1; t++)
                relax.add(IloConversion(env, y[j][t], ILOFLOAT));

        for (int i = 0; i < d.J; i++)
            for (int j = 0; j < d.J; j++)
                for (int t = 0; t < d.T; t++)
                    relax.add(IloConversion(env, z[i][j][t], ILOFLOAT));

        return relax;
    }

    // Le da solucao do CPLEX os valores de y, gama e z
    void le(IloCplex &cplex, Solucao &s){
        for (int j = 0; j < d.J; j++)
            for (int t = 0; t < d.T; t++)
                s.y[j][t] = cplex.getValue(y[j][t]);

        for (int n = 0; n < d.N; n++)
            for (int t = 0; t < d.T; t++)
                s.gama[n][t] = cplex.getValue(gama[n][t]);

        for (int i = 0; i < d.J; i++)
            for (int j = 0; j < d.J; j++)
                for (int t = 0; t < d.T; t++)
                    s.z[i][j][t] = cplex.getValue(z[i][j][t]);
    }

    // Fixa em 1 os gama que valem 1 na solucao s
    void fixaPedidos(const Solucao &s){
        for (int n = 0; n < d.N; n++)
            for (int t = 0; t < d.T; t++)
                if (s.gama[n][t] >= 0.5){
                    gama[n][t].setLB(1);
                    gama[n][t].setUB(1);
                }
    }

    // Libera os gama: volta aos limites originais [0, 1]
    void liberaPedidos(){
        for (int n = 0; n < d.N; n++)
            for (int t = 0; t < d.T; t++){
                gama[n][t].setLB(0);
                gama[n][t].setUB(1);
            }
    }

    // Fixa em 1 os y e z que valem 1 na solucao s (melhores sequencias)
    void fixaSequencia(const Solucao &s){
        for (int j = 0; j < d.J; j++)
            for (int t = 0; t < d.T; t++)
                if (s.y[j][t] >= 0.5){
                    y[j][t].setLB(1);
                    y[j][t].setUB(1);
                }

        for (int i = 0; i < d.J; i++)
            for (int j = 0; j < d.J; j++)
                for (int t = 0; t < d.T; t++)
                    if (s.z[i][j][t] >= 0.5){
                        z[i][j][t].setLB(1);
                        z[i][j][t].setUB(1);
                    }
    }
};

/*-----------------------------------------------------------------------------------------------------------------------------------------
    HeuristicaM3: os pedidos sao ordenados pelo custo reduzido de gama na relaxacao linear (nao-crescente) e aceitos um a um,
    mantendo cada pedido apenas se a solucao melhorar.
    Depois, a FASE 2 resolve o modelo completo com o tempo restante.
-------------------------------------------------------------------------------------------------------------------------------------------
*/
const double LIMITE_FASE1 = 1800; // tempo limite do laco da FASE 1, em segundos
const double TILIM = 100;         // tempo limite de cada resolucao do CPLEX na FASE 1
const double DIVISOR_FASE2 = 4;   // a FASE 2 usa (3600 - tempo da FASE 1)/DIVISOR_FASE2 como limite

class HeuristicaM3 {
    IloEnv env;
    const Instancia &d;
    Modelo &modelo;
    Cronometro relogio;

    double melhorFO;
    Solucao atual, melhor;

    // Ordena os valores v em forma decrescente, acompanhando o vetor de indices idx (se nao for nulo).
    // O valor trocado passa por um int, como no codigo original (trunca a parte fracionaria).
    static void ordenaDecrescente(vector<double> &v, vector<int> *idx){
        int aux;
        int aux2;
        for (size_t i = 0; i < v.size(); i++){
            for (size_t j = 0; j < v.size(); j++){
                if (v[j] < v[i]){
                    aux = v[j];
                    v[j] = v[i];
                    v[i] = aux;
                    if (idx){
                        aux2 = (*idx)[j];
                        (*idx)[j] = (*idx)[i];
                        (*idx)[i] = aux2;
                    }
                }
            }
        }
    }

    // Pedidos em ordem nao-crescente do melhor custo reduzido de gama na relaxacao linear
    vector<int> ordenaPorCustoReduzido(IloCplex &relax){
        vector<double> melhoresRC(d.N, 0);
        vector<int> pedidos(d.N);

        for (int n = 0; n < d.N; n++){
            //Captura os custos reduzidos de todos periodos em cada pedido
            vector<double> rc(d.T);
            for (int t = 0; t < d.T; t++)
                rc[t] = relax.getReducedCost(modelo.gama[n][t]);

            //Captura os melhores custos reduzidos de cada pedido
            ordenaDecrescente(rc, NULL);

            melhoresRC[n] = rc[0];
            pedidos[n] = n;
        }

        ordenaDecrescente(melhoresRC, &pedidos);
        return pedidos;
    }

    // Remove as restricoes de "pedido nao atendido" adicionadas para o teste
    void removeZeros(IloModel &modelo1, IloConstraintArray &adicionarzero){
        for (int n = 0; n < d.N; n++)
            modelo1.remove(adicionarzero[n]);
    }

    // FASE 2: resolve o modelo completo com o tempo restante, fixando pedidos e depois sequencias
    void fase2(double tempo1){
        IloModel modelo2 = modelo.construir();
        IloCplex cplex2(env);
        cplex2.extract(modelo2);
        cplex2.setParam(IloCplex::Param::TimeLimit, (3600 - tempo1)/DIVISOR_FASE2);

        //Inicio da FASE 2
        //Primeira resolucao: se melhorar, fixa os melhores pedidos
        if (cplex2.solve()){
            modelo.le(cplex2, atual);

            if (cplex2.getObjValue() >= melhorFO){
                melhorFO = cplex2.getObjValue();
                melhor = atual;

                //MELHORES PEDIDOS
                modelo.fixaPedidos(melhor);
            }
        }

        //Segunda resolucao: se melhorar, libera os pedidos e fixa as melhores sequencias
        if (cplex2.solve()){
            modelo.le(cplex2, atual);

            if (cplex2.getObjValue() >= melhorFO){
                melhorFO = cplex2.getObjValue();
                melhor = atual;

                modelo.liberaPedidos();

                //MELHORES SEQUENCIAS
                modelo.fixaSequencia(melhor);
            }
        }

        //Terceira resolucao: resultado final
        bool resolveu = cplex2.solve();
        double tempo2 = relogio.decorrido() - tempo1;

        if (resolveu && cplex2.getObjValue() >= melhorFO)
            melhorFO = cplex2.getObjValue();

        printf(" Melhor solucao encontrada na FASE 2: %.f \n", melhorFO);
        printf(" Tempo de execucao da FASE 2: %.2f \n", tempo2);
    }

public:
    HeuristicaM3(IloEnv e, const Instancia &inst, Modelo &m)
        : env(e), d(inst), modelo(m), melhorFO(-999999), atual(inst), melhor(inst) {}

    void executa(){
        IloModel modelo1 = modelo.construir();

        IloCplex fase1(modelo1);
        fase1.extract(modelo1);
        fase1.setParam(IloCplex::Param::TimeLimit, TILIM);

        //Resolver o problema relaxado
        IloModel relax = modelo.relaxar(modelo1);
        IloCplex RELAX(relax);
        if (!RELAX.solve()){
            env.error() << "Nao se pode resolver" << endl;
            throw(-1);
        }

        vector<int> vetor = ordenaPorCustoReduzido(RELAX);

        relogio.reinicia();

        std::set<int> conjunto;

        //Heuristica - FO5
        for (int k = 0; k < d.N && relogio.decorrido() <= LIMITE_FASE1; k++){

            // Adicionar restricao no modelo fixada em 1
            IloExpr restricao1(env);
            for (int t = d.F[vetor[k]]; t <= d.L[vetor[k]]; t++)
                restricao1 += modelo.gama[vetor[k]][t];

            IloConstraint adicionarum = restricao1 == 1;
            modelo1.add(adicionarum);
            restricao1.end();
            conjunto.insert(vetor[k]);

            // Adicionar restricao fixando em zero os pedidos que ainda nao foram aceitos
            IloConstraintArray adicionarzero(env, d.N);
            for (int n = 0; n < d.N; n++){
                IloExpr restricao0(env);
                for (int t = d.F[n]; t <= d.L[n]; t++)
                    restricao0 += modelo.gama[n][t];

                adicionarzero[n] = restricao0 == 0;
                restricao0.end();

                if (conjunto.find(n) == conjunto.end())
                    modelo1.add(adicionarzero[n]);
            }

            //Se com a adicao da restricao o problema for factivel
            if (fase1.solve()){
                modelo.le(fase1, atual);

                //Comparar valor atual da solucao com o valor anterior
                double atualFO = fase1.getObjValue();
                if (atualFO > melhorFO){
                    melhorFO = atualFO;
                    removeZeros(modelo1, adicionarzero);
                    melhor = atual;
                    modelo.fixaPedidos(melhor);
                }
                //Se a solucao piorar, remover a restricao adicionada
                else {
                    removeZeros(modelo1, adicionarzero);
                    modelo1.remove(adicionarum);
                    conjunto.erase(vetor[k]);
                }
            }
            //Se o problema for infactivel, remover a restricao recem adicionada
            else {
                removeZeros(modelo1, adicionarzero);
                modelo1.remove(adicionarum);
                conjunto.erase(vetor[k]);
            }
        }

        double tempo1 = relogio.decorrido();

        printf(" Melhor solucao encontrada na FASE 1: %.f \n", melhorFO);
        printf(" Tempo de execucao da FASE 1: %.2f \n", tempo1);

        fase2(tempo1);
    }
};

int main(int argc, char **argv){
    IloEnv env;

    try{
        Instancia instancia(env, argv[1]);
        Modelo modelo(env, instancia);
        HeuristicaM3 heuristica(env, instancia, modelo);
        heuristica.executa();
    }
    catch(IloException& ex) {
        cerr << "ERRO: " << ex << endl;
    }
    catch(...) {
        cerr << "ERRO!" << endl;
    }

    env.end();
    return 0;
}
