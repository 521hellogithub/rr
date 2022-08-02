/* -*- Mode: C; tab-width: 8; c-basic-offset: 2; indent-tabs-mode: nil; -*- */

#include "util.h"

#include <link.h>

/* This test checksums all writable memory mappings of the process and writes it
   to stdout. This might seem like an odd thing to do, but it's a reasonably close
   match to memory-scanning infrastructure such as leak detectors and primitive
   garbage collectors. We want to avoid any divergent memory contents from being
   visible to such processes. */

static uint32_t crc32c_sw(uint32_t crci, const char *buf, size_t len);
static int dl_iterate_phdr_cb(struct dl_phdr_info *info, size_t size, void *crcptr) {
  uint32_t *crc = (uint32_t*)crcptr;
  for (size_t i = 0; i < info->dlpi_phnum; i++) {
    const ElfW(Phdr) *phdr = &info->dlpi_phdr[i];
    if ((phdr->p_type != PT_LOAD) || (phdr->p_memsz == 0))
      continue;
    /* This specifically exludes the rr_page code section, which differs between record and replay */
    if (!(phdr->p_flags & PF_W))
      continue;
    *crc = crc32c_sw(*crc, (char *)(info->dlpi_addr + phdr->p_vaddr), phdr->p_memsz);
  }
  (void)size;
  return 0;
}

int main(void) {
  uint32_t crc;
  dl_iterate_phdr(dl_iterate_phdr_cb, &crc);
  atomic_printf("Checksum: 0x%08x\n", crc);
  atomic_printf("EXIT-SUCCESS\n");
  return 0;
}

/********************** crc32c Implementation **********************/

/*
  Copyright (C) The Julia contributors.
  Copyright (C) 2013 Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the author be held liable for any damages
  arising from the use of this software.
  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:
  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
  Mark Adler
  madler@alumni.caltech.edu
*/
static const uint32_t crc32c_table[8][256] = {
    { 0,4067132163,3778769143,324072436,3348797215,904991772,648144872,3570033899,2329499855,2024987596,1809983544,2575936315,1296289744,3207089363,2893594407,1578318884,274646895,3795141740,4049975192,51262619,3619967088,632279923,922689671,3298075524,2592579488,1760304291,2075979607,2312596564,1562183871,2943781820,3156637768,1313733451,549293790,3537243613,3246849577,871202090,3878099393,357341890,102525238,4101499445,2858735121,1477399826,1264559846,3107202533,1845379342,2677391885,2361733625,2125378298,820201905,3263744690,3520608582,598981189,4151959214,85089709,373468761,3827903834,3124367742,1213305469,1526817161,2842354314,2107672161,2412447074,2627466902,1861252501,1098587580,3004210879,2688576843,1378610760,2262928035,1955203488,1742404180,2511436119,3416409459,969524848,714683780,3639785095,205050476,4266873199,3976438427,526918040,1361435347,2739821008,2954799652,1114974503,2529119692,1691668175,2005155131,2247081528,3690758684,697762079,986182379,3366744552,476452099,3993867776,4250756596,255256311,1640403810,2477592673,2164122517,1922457750,2791048317,1412925310,1197962378,3037525897,3944729517,427051182,170179418,4165941337,746937522,3740196785,3451792453,1070968646,1905808397,2213795598,2426610938,1657317369,3053634322,1147748369,1463399397,2773627110,4215344322,153784257,444234805,3893493558,1021025245,3467647198,3722505002,797665321,2197175160,1889384571,1674398607,2443626636,1164749927,3070701412,2757221520,1446797203,137323447,4198817972,3910406976,461344835,3484808360,1037989803,781091935,3705997148,2460548119,1623424788,1939049696,2180517859,1429367560,2807687179,3020495871,1180866812,410100952,3927582683,4182430767,186734380,3756733383,763408580,1053836080,3434856499,2722870694,1344288421,1131464017,2971354706,1708204729,2545590714,2229949006,1988219213,680717673,3673779818,3383336350,1002577565,4010310262,493091189,238226049,4233660802,2987750089,1082061258,1395524158,2705686845,1972364758,2279892693,2494862625,1725896226,952904198,3399985413,3656866545,731699698,4283874585,222117402,510512622,3959836397,3280807620,837199303,582374963,3504198960,68661723,4135334616,3844915500,390545967,1230274059,3141532936,2825850620,1510247935,2395924756,2091215383,1878366691,2644384480,3553878443,565732008,854102364,3229815391,340358836,3861050807,4117890627,119113024,1493875044,2875275879,3090270611,1247431312,2660249211,1828433272,2141937292,2378227087,3811616794,291187481,34330861,4032846830,615137029,3603020806,3314634738,939183345,1776939221,2609017814,2295496738,2058945313,2926798794,1545135305,1330124605,3173225534,4084100981,17165430,307568514,3762199681,888469610,3332340585,3587147933,665062302,2042050490,2346497209,2559330125,1793573966,3190661285,1279665062,1595330642,2910671697 },
    { 0,329422967,658845934,887597209,1317691868,1562966443,1775194418,2054015301,2635383736,2394315727,3125932886,2851302177,3550388836,3225172499,4108030602,3883469565,1069937025,744974838,411091311,186800408,1901039709,1659701290,1443537075,1168652484,2731618873,2977147470,2241069783,2520160928,3965408229,4294560658,3407766283,3636263804,2139874050,1814657909,1489949676,1265388443,822182622,581114537,373600816,98970183,3802079418,4047354061,3319402580,3598223395,2887074150,3216496913,2337304968,2566056447,1078858371,1408010996,1728782957,1957280282,247755615,493284136,696337329,975428550,3713716539,3472378188,4196393429,3921508770,2479927527,2154965136,3029696521,2805405822,4279748100,3971309171,3629315818,3421531805,2979899352,2722054063,2530776886,2239369025,1644365244,1906417099,1162229074,1457827109,747201632,1059847191,197940366,409914617,3235002245,3547377650,3885434731,4097154844,2388153945,2650459694,2837276343,3133144768,1573319741,1315204170,2055455955,1763794084,323786209,15601046,873047311,665533816,2157716742,2470362481,2816021992,3027996063,3457565914,3719617709,3914560564,4210158659,495511230,237665993,986568272,695160359,1392674658,1084235541,1950857100,1743073275,3210335367,2902150384,2552030313,2344516638,4057183579,3799067948,3600188853,3308527042,575477567,837783368,84420561,380288934,1825011427,2137386644,1266828813,1478549114,4223924985,3898696334,3699821079,3475264096,3041499941,2800419666,2450303947,2175677372,1725380929,1970643254,1100089775,1378914776,677206173,1006616810,253257843,482013188,3288730488,3617886991,3812834198,4041319393,2324458148,2569990867,2915654218,3194733117,1494403264,1253068983,2119694382,1844797529,395880732,70922603,819829234,595526021,2219317755,2548728204,2735548693,2964304226,3401742375,3647004752,3985066185,4263891134,425515587,184435252,1041885869,767259354,1473690527,1148462056,1888717681,1664160518,3146639482,2821681165,2630408340,2406105315,4110911910,3869577681,3527588168,3252691263,647572418,893105077,31202092,310281051,1746094622,2075251305,1331067632,1559552647,81018109,393651338,596708371,808686692,1247698209,1509737814,1830514127,2126116280,2579562309,2321704754,3196440491,2905036764,3611991705,3303540462,4027559543,3819779584,991022460,682841355,475331986,267806181,1973136544,1715025111,1390320718,1098646585,2785349316,3047659187,2168471082,2464327261,3901714200,4214093679,3486146550,3697854337,2069880831,1761429384,1545269009,1337489254,903200291,645342804,311463629,20059834,3863682119,4125721648,3238931625,3534533854,2831252891,3143886316,2407812469,2619790594,1150955134,1463334409,1675566736,1887274727,168841122,431151061,760577868,1056433979,3650022854,3391911345,4274773288,3983099231,2533657626,2225476717,2957098228,2749572227 },
    { 0,2772537982,1332695565,3928932467,2665391130,1000289892,3518101015,1961911401,944848581,2635115707,2000579784,3531603638,2794429151,63834273,3923822802,1285642924,1889697162,3588485108,1070411655,2592914937,4001159568,1262308334,2702412701,72489443,1223902031,3987919153,127668546,2732426044,3593332565,1936487723,2571285848,1006839590,3779394324,1141205354,2922096921,191511399,2140823310,3671838064,821366019,2511642493,3642082769,2085902255,2524616668,859506082,1204511179,3800757173,144978886,2917507512,2447804062,883365088,3733574803,2076722925,255337092,2860101882,1079472265,3843482359,2847389787,217459237,3872975446,1134131240,929635393,2452131391,2013679180,3712474162,3345318105,1646531239,2282410708,759906474,1505436867,4244289213,383022798,3012945072,4281646620,1517628514,2958814225,354057839,1642732038,3299575928,780486667,2344934005,3083337043,310800173,4171804510,1575566624,689527113,2354629431,1719012164,3275200826,2409022358,718754280,3237581211,1706558437,289957772,3020551666,1579627905,4217808895,639728589,2204166579,1766730176,3423583166,3103776727,499010985,4153445850,1389436836,510674184,3140605814,1360992005,4099835259,2158944530,636449644,3485578015,1786782049,1451427399,4089615417,434918474,3165505076,3361579613,1830563875,2268262480,577987118,1859270786,3415452412,566061711,2231171313,4027358360,1431113446,3210989205,438459627,2334619459,778495293,3293062478,1628026672,368694105,2964865319,1519812948,4292285226,3010873734,372759544,4229503883,1498974709,766045596,2297004002,1657257873,3347459567,4219800265,1589942455,3035257028,296471226,1700507347,3222944941,708115678,2406837920,3285464076,1721083506,2361091585,704312447,1560973334,4165665384,308658715,3072610405,1784908887,3475119657,621600346,2152549412,4106037325,1375517235,3151133248,513009726,1379054226,4151517420,492691615,3088872161,3438024328,1772979446,2206418053,650303227,448917981,3212862371,1437508560,4042207662,2216646087,559859641,3413116874,1848743348,579915544,2278645094,1845468437,3367898987,3159255810,420477308,4079040783,1449175921,1279457178,3909314020,53323159,2792110057,3533460352,2011021822,2649948557,951227379,1947453791,3511835425,998021970,2654800172,3939331397,1334640443,2778873672,14921014,1021348368,2577471598,1938806813,3603843683,2721984010,125811828,3981540359,1209069177,78755029,2716870315,1272899288,4003427494,2590970063,1060012721,3573564098,1883361468,2902854798,138911472,3798556291,1193856253,869836948,2526624490,2092432025,3656804583,2505519691,806789173,3661127750,2138698296,193566289,2932343855,1155974236,3785840162,3718541572,2028331898,2462786313,931836279,1132123422,3862644576,202737427,2840860013,3858059201,1085595071,2862226892,266047410,2066475995,3731519909,876919254,2433035176 },
    { 0,3712330424,3211207553,1646430521,2065838579,2791807819,3292861042,419477706,4131677158,721537374,1227047015,2489772767,2372293141,1344534701,838955412,4014267180,3915690301,874584965,1443074748,2336634884,2454094030,1325607542,757179215,4033087991,522244827,3261429859,2689069402,2097306594,1677910824,3108456848,3680878761,102787601,3609531531,174112307,1749169930,3037175218,2886149496,1900187584,325060345,3458575425,560035693,4230274517,2651215084,1128529492,1514358430,2265377830,3844367647,945934247,1044489654,3808694030,2166785591,1550003343,1164153925,2552643325,4194613188,658578812,3355821648,356543720,2002970065,2854702953,3005740963,1851940123,205575202,3506798234,2879807463,1994650975,348224614,3380926174,3498339860,230802604,1877167509,2997282605,1575107585,2158466745,3800375168,1069593912,650120690,4219840330,2577870451,1155695819,1120071386,2676442210,4255501659,551577571,971038505,3836048785,2257058984,1539462672,3028716860,1774397316,199339709,3601073157,3483679951,316741239,1891868494,2911254006,2088979308,2714165716,3286526189,513917525,128023199,3672428583,3100006686,1703146406,2328307850,1468170802,899681035,3907363251,4058323321,748729281,1317157624,2479329344,2515008081,1218597097,713087440,4156912488,4005940130,864051482,1369630755,2363966107,1671666103,3202757391,3703880246,25235598,411150404,3317957372,2816904133,2057511293,1386268991,2414175111,3989301950,813842438,696449228,4106703476,2531646285,1268806133,2766449369,2040594529,461605208,3334874080,3754335018,42152338,1621211307,3185840659,3150215170,1719784122,77814659,3655790907,3236317681,497279817,2139187824,2730803400,1300241380,2428875100,4075239525,799183581,916597271,3957817519,2311391638,1417716526,2240142772,1489008396,987954741,3886503053,4272417863,602031871,1103155142,2625987966,1942077010,2927891690,3433471443,300103531,149131169,3584435481,3078925344,1791035032,1826712713,2980365873,3548794632,247719344,398679418,3397842882,2829352699,1977734211,2594508655,1205904855,633482478,4169631318,3783736988,1019384868,1591745821,2208675749,4177958616,608386144,1180808537,2602835937,2183440171,1600195987,1027835050,3758501394,256046398,3523698566,2955269823,1835039751,1952498893,2837802613,3406292812,373444084,274868197,3441921373,2936341604,1916841692,1799362070,3053829294,3559339415,157458223,3861267459,996404923,1497458562,2214907194,2634315248,1078058824,576935537,4280745161,774079059,4083558635,2437194194,1275136874,1426174880,2286164248,3932590113,925055641,3630686645,86133517,1728102964,3125110924,2739261510,2113960702,472052679,3244775807,3343332206,436378070,2015367407,2774907479,3160736413,1629530149,50471196,3729230756,822300808,3964074544,2388947721,1394727345,1243701627,2539965379,4115022586,671344706 },
    { 0,940666796,1881333592,1211347188,3762667184,3629437212,2422694376,2826309188,3311864721,4252394557,3041252553,2371140453,623031585,489937549,1426090617,1829832149,2401395155,3073576575,4278238859,3339833639,1869078371,1467396303,524615739,659845015,1246063170,1918109166,979875098,41343670,2852181234,2450635614,3659664298,3795018758,464041303,599382779,1804233231,1402668451,4226616295,3288097867,2345653439,3017718547,3738156742,3873362282,2934792606,2533101106,1049231478,110850010,1319690030,1991880834,2492126340,2895887144,3836218332,3703137392,1959750196,1289618840,82687340,1023204032,1374543637,1778167993,567214157,434008033,2980602277,2310606345,3247095549,4187738449,928082606,255853826,1198765558,2137184858,3608466462,4010130354,2805336902,2670159082,4063650111,3391557267,2182397543,3120943563,309583759,711110691,1649475799,1514172283,3094547325,2153931985,3360805925,4030774153,1479980493,1613224545,672426645,268764473,2098462956,1157984064,221700020,891793432,2639380060,2772488688,3983761668,3579973288,754573305,350793813,1557856417,1690988301,3434897737,4104982245,3164519953,2224017853,3919500392,3515857860,2579237680,2712495260,165374680,835323252,2046408064,1105779244,2749087274,2613760390,3556335986,3957853918,1134428314,2072997686,868016066,195932270,1723643323,1588451863,379480803,781124943,2264468235,3202901159,4141601875,3469392895,1856165212,1454619376,511707652,647062952,2397531116,3069576256,4274369716,3335838488,2881871565,2480189793,3689349525,3824578105,1266704509,1938886609,1000521509,62115977,3783308431,3650214691,2443340759,2847081595,29690431,970220947,1911018855,1240906443,619167518,485937330,1422221382,1825837034,3298951598,4239617538,3028344566,2358358362,1963614219,1293619111,86556499,1027199231,2505039547,2908664087,3849126371,3715919439,2959960986,2289828918,3226449090,4166966126,1344853290,1748613766,537528946,404448734,4196925912,3258543732,2315968128,2988159276,443400040,578605252,1783586864,1381896092,1062144585,123626981,1332598033,2004662973,3742020857,3877362517,2938661793,2537096205,1509146610,1642254430,701587626,297799430,3115712834,2175233774,3381976602,4052070838,2626991203,2760235983,3971377979,3567715479,2094074579,1153459583,217306507,887274023,3604078113,4005605773,2800943481,2665639637,915693713,243601213,1186381769,2124927077,330749360,732412444,1670646504,1535468868,4092816128,3420587180,2211558488,3149978612,1113262757,2051695881,846845437,174635601,2719921173,2584730553,3527174989,3928818913,2268856628,3207425688,4145995372,3473912256,1736032132,1600704552,391864540,793382768,3447286646,4117234906,3176903726,2236275586,758961606,355318378,1562249886,1695507762,136208615,806293323,2017247167,1076744211,3898334807,3494556155,2558066959,2691198627 },
    { 0,4012927769,3683426499,884788186,3002414967,1573215342,1769576372,2252995757,1611012127,2402710278,3146430684,1421530053,3539152744,1036207217,159354795,3863995570,3222024254,792484647,461410557,4105239524,1928922953,2647223376,2843060106,1178979475,2685020193,1329218360,2072414434,2495013883,318709590,4258231375,3379806101,641979532,2247366285,1791262100,1584969294,2974342487,922821114,3627109091,3968696633,62777888,3857845906,180512139,1048489553,3511600456,1460091365,3090633468,2357958950,1673261631,1173890739,2865253802,2658436720,1900342633,4144828868,406682333,746696967,3283212830,637419180,3402519989,4268924527,289600886,2534083035,2017157826,1283959064,2746728961,235166699,3778294002,3582524200,985174065,3169938588,1405159301,1736297567,2286790470,1845642228,2167548141,3046040375,1522436142,3707204739,868687770,125555776,3897278297,3456658389,557318348,361024278,4206141455,2096979106,2479699899,2809265249,1212258168,2920182730,1094588627,1971507977,2595403792,486229181,4090179492,3346523262,675778407,2347781478,1690314367,1350364581,3209463484,956660241,3593801992,3800685266,230273483,3958789497,80100960,813364666,3746209443,1493393934,3056797975,2190459597,1841277396,1274838360,2764838465,2423315867,2134947458,4178135599,372842806,579201772,3451224565,737830215,3301576286,4034315652,524725917,2567918128,1983854889,1115943667,2914228714,470333398,4080590031,3347322645,682916876,2935849121,1104014264,1970348130,2587970427,2081337289,2470364368,2810318602,1219650579,3472595134,567014311,360134781,4198978404,3691284456,859106545,126363435,3904392242,1861333151,2176965510,3044872284,1515027269,3154288631,1395848430,1737375540,2294174765,251111552,3787965337,3581610051,978019162,2583470427,1993132610,1114636696,2906679937,722048556,3292134709,4035262191,531979766,4193958212,382390877,578165127,3443946142,1259310643,2755650858,2424516336,2142455273,1508938085,3066100348,2189177254,1833720511,3943015954,70634763,814286545,3753471432,972458362,3603358307,3799656889,222970528,2332278285,1681118484,1351556814,3216995799,302836797,4248602404,3380628734,649078759,2700729162,1338617939,2071296905,2487554192,1913320482,2637864763,2844153057,1186349048,3237987157,802138188,460546966,4098033807,3523271683,1026602778,160201920,3871086553,1626729332,2412085357,3145288631,1414078638,2986787868,1563864837,1770677471,2260340678,15987563,4022573170,3682554792,877607089,2549676720,2026410409,1282693747,2739155306,621661639,3393038046,4269894916,296814109,4160676527,416188854,745685612,3275893109,1158403544,2856042177,2659677467,1907826178,1475660430,3099894167,2356701773,1665663316,3842113017,171022048,1049451834,3518838307,938660497,3636640136,3967709778,55449931,2231887334,1782025983,1586185509,2981834300 },
    { 0,1745038536,3490077072,3087365464,2782971345,3454265625,1978047553,501592201,1311636819,640602523,2653660355,4129851403,3956095106,2211320906,1003184402,1405636058,2623273638,4099462766,1281205046,610177022,968572791,1371018175,3921503975,2176731695,3530950645,3128240957,40918629,1785950893,2006368804,529919724,2811272116,3482564476,1029407677,1431875445,3982350893,2237593317,2562410092,4038617764,1220354044,549335860,1937145582,460673574,2742036350,3413314486,3600202559,3197474807,110158511,1855180391,2701162779,3372438995,1896226955,419761219,81837258,1826852866,3571901786,3169175954,4012737608,2267981952,1059839448,1462300944,1254965657,583953745,2597001225,4073206977,2058815354,313797554,2863750890,3266474530,3747070635,3075796579,257006395,1733474291,882571817,1553585889,3835470777,2359267185,2440708088,4185461552,1098671720,696208032,3874291164,2398081300,921347148,1592363140,1124874253,722408645,2466931101,4211690837,2831220879,3233950791,2026330399,281310679,220317022,1696786838,3710360782,3039080454,1206682823,804235279,2548751703,4293521823,3792453910,2316266974,839522438,1510552654,163674516,1640125788,3653705732,2982415564,2887892037,3290599565,2082989525,337955101,3686235745,3014939305,196159473,1672612665,2119678896,374642552,2924601888,3327315688,2509931314,4254707706,1167907490,765458026,813319907,1484352043,3766230899,2290037691,4117630708,2641175100,627595108,1298889644,1351535397,948824045,2156434101,3901472381,3141792679,3544244079,1799727671,54953727,514012790,1990204094,3466948582,2795914030,1765143634,20371610,3107171778,3509616906,3436523907,2765495627,483616787,1959806171,655886593,1327179209,4145959057,2669509721,2197343440,3942375448,1392416064,989706632,3358952777,2687934849,406050009,1882257425,1842694296,97936464,3184726280,3587194304,2249748506,3994770642,1444817290,1042089282,603502027,1274779907,4093570139,2617098387,1416525807,1013799719,2221420159,3966436023,4052660798,2576195318,562621358,1233897318,440634044,1916839540,3393573676,2722562020,3215150957,3617612709,1873090301,128334389,2413365646,3889833286,1608470558,937196758,708430943,1111154839,4198471119,2453453063,3254056157,2851592213,301116749,2045870469,1679044876,202841540,3021105308,3692119124,327349032,2072109024,3280251576,2877785712,3059889913,3730905649,1717858153,241648545,1571753595,900473523,2376685547,3853155107,4165979050,2420959074,675910202,1078640370,2994899507,3665929979,1652872099,176684907,392318946,2137088810,3345225330,2942778042,4239357792,2494323624,749285104,1151992376,1498395313,827104889,2303322913,3779774441,786002069,1188715613,4276037893,2531001805,2335814980,3812268428,1530916052,859619356,1626639814,150446350,2968704086,3639736478,3306440727,2903991519,353505671,2098281807 },
    { 0,1228700967,2457401934,3678701417,555582061,1747058506,3009771555,4200137988,1111164122,185039357,3494117012,2575270835,1663469239,706411408,4049501433,3093430750,2222328244,3444208787,370078714,1597148893,2775288793,3965187838,924021143,2117012656,3326938478,2406576201,1412822816,487164423,3880816387,2926375460,1965585741,1007945834,218129817,1144789182,2675482583,3594838768,740157428,1696701139,3194297786,4149829789,1329291587,101129316,3712195341,2491409962,1848042286,656055817,4234025312,3043124295,2306239533,3226079498,453940835,1379068740,2825645632,3780612967,974328846,1932486953,3410847991,2188449232,1496683193,269086622,3931171482,2741802941,2015891668,823422451,436259634,1396487701,2289578364,3242478683,991775071,1914778744,2842014481,3763981878,1480314856,285717199,3393402278,2206156929,2032553349,807022754,3948853195,2724383468,2658583174,3612000161,202258632,1160922607,3211477227,4132912588,756267685,1680852866,3696084572,2507258747,1312111634,118047029,4249895985,3026991382,1864941183,638894936,385920683,1581044620,2239255781,3427019202,907881670,2132890081,2758137480,3982076847,1429973617,470275926,3343077439,2390699288,1948657692,1025135931,3864973906,2942480245,2474026783,3662338616,17718609,1211244662,2993366386,4216805461,538173244,1764729371,3511526341,2557599458,1127569803,168371372,4031783336,3110886543,1646844902,722773697,872519268,2101209923,2792975402,4014280973,354161673,1545627950,2271538759,3461911392,1983550142,1057419161,3829557488,2910721495,1462178003,505114100,3311397533,2355337146,2960629712,4182500087,571434398,1798510777,2439650749,3629539482,51570675,1244568276,4065106698,3144738349,1614045508,688397411,3545307495,2590860352,1093264169,135634446,956429309,1883082458,2876836275,3796202644,404517264,1361054903,2321845214,3277387513,2067461927,839289344,3913420137,2692640846,1512535370,320538733,3361705732,2170810915,3178756681,4098590574,789512199,1714650400,2624223268,3579184387,236094058,1194262349,4283235987,3060827060,1832125661,604535290,3729882366,2540503513,1277789872,85326743,771841366,1732059249,3162089240,4114995775,253550395,1176543772,2640586101,3562559570,1815763340,621159595,4265780162,3078545125,1294457825,68921030,3747553711,2523094152,2859947234,3813353925,940551852,1899221899,2339034767,3260459944,420621505,1345212902,3897315384,2708483359,2050271862,856217425,3377582677,2154671986,1529423899,303387964,587282639,1782400488,2977546881,4165320614,35437218,1260439429,2422489324,3646438859,1631206421,671498546,4081239643,3128867708,1076346488,152814431,3529458742,2606971153,2809606523,3997912156,890227509,2083763730,2255139606,3478572593,336742744,1563309183,3846976929,2893039750,1999949807,1040757448,3293689804,2372782827,1445547394,521482405 }
};

static uint32_t crc32c_sw(uint32_t crci, const char *buf, size_t len)
{
    uintptr_t crc = crci ^ 0xffffffff;
    while (len && ((uintptr_t)buf & 7) != 0) {
        crc = crc32c_table[0][(crc ^ *buf++) & 0xff] ^ (crc >> 8);
        len--;
    }
    while (len >= 8) {
#if defined(__x86_64__) || defined(__aarch64__)
        crc ^= *(uint64_t*)buf;
        crc = crc32c_table[7][crc & 0xff] ^
            crc32c_table[6][(crc >> 8) & 0xff] ^
            crc32c_table[5][(crc >> 16) & 0xff] ^
            crc32c_table[4][(crc >> 24) & 0xff] ^
            crc32c_table[3][(crc >> 32) & 0xff] ^
            crc32c_table[2][(crc >> 40) & 0xff] ^
            crc32c_table[1][(crc >> 48) & 0xff] ^
            crc32c_table[0][crc >> 56];
#else
        uint32_t *p = (uint32_t*)buf;
        crc ^= p[0];
        uint32_t hi = p[1];
        crc = crc32c_table[7][crc & 0xff] ^
            crc32c_table[6][(crc >> 8) & 0xff] ^
            crc32c_table[5][(crc >> 16) & 0xff] ^
            crc32c_table[4][(crc >> 24) & 0xff] ^
            crc32c_table[3][hi & 0xff] ^
            crc32c_table[2][(hi >> 8) & 0xff] ^
            crc32c_table[1][(hi >> 16) & 0xff] ^
            crc32c_table[0][hi >> 24];
#endif
        buf += 8;
        len -= 8;
    }
    while (len) {
        crc = crc32c_table[0][(crc ^ *buf++) & 0xff] ^ (crc >> 8);
        len--;
    }
    return (uint32_t)crc ^ 0xffffffff;
}
